/**
 * @file scene.cpp
 * @brief The command decoder and the retained scene reconciler.
 *
 * Two subsystems that only look like one: a transport, and a diff.
 *
 * The transport is a flat array of 32-bit words. It exists because a per-entity
 * FFI call per frame would eat exactly the win the SoA layout was built to get
 * (ARCHITECTURE.md §6.7), so the boundary is crossed once with everything
 * packed rather than a thousand times with one field each.
 *
 * The diff exists because Dart's hot reload re-runs build() without unwinding
 * native state (§6.6). Imperative construction therefore doubles the scene on
 * every reload. A description plus a reconciler cannot: submitting the same
 * description twice reaches the same live scene, because the second pass finds
 * every key already mapped and has nothing to do.
 *
 * Nothing here touches EshiWorld's layout. The reconciler drives the public C
 * API — the same one Dart will call — which keeps it honest about whether that
 * API can actually describe a scene.
 */
#include "eshi/eshi.h"
#include "eshi_internal.h"

#include <cstring>
#include <map>
#include <new>
#include <vector>

namespace {

/*
 * A described node.
 *
 * Payloads are kept as raw words rather than decoded floats, for two reasons.
 * Change detection becomes an integer compare, which sidesteps float equality
 * entirely: NaN != NaN would otherwise make every reconcile re-fire a value
 * that never changed, and -0.0 == 0.0 would hide one that did. And the compare
 * happens against the bytes the writer actually sent, so it answers "did the
 * description change" rather than "did the decoded value change".
 *
 * This is AoS where the rest of the core is SoA. Deliberate: the component
 * arrays are walked every frame by systems, this table is walked when a scene
 * is submitted. Different access pattern, different layout.
 */
struct SceneNode {
    uint32_t   key;
    EshiEntity entity;
    uint32_t   seen;      /* epoch this node was last described in */
    uint32_t   described; /* bit per opcode: which payloads below are valid */

    uint32_t transform[2];
    uint32_t velocity[2];
    uint32_t collider[5];
    uint32_t restitution[1];
    uint32_t bounds[4];
};

} /* namespace */

/*
 * std::map, not unordered_map, and the reason is determinism rather than taste.
 * The sweep and every read-back walk `nodes` — insertion-ordered and therefore
 * reproducible — but a hash container's *rehash* behaviour differs between
 * libc++ and libstdc++, and the day someone iterates the index instead of the
 * dense array, an ordered container makes that a portable bug rather than a
 * platform-specific one. Scene submissions are rare; the log factor is free.
 */
struct EshiSceneState {
    std::vector<SceneNode>       nodes;
    std::map<uint32_t, int32_t>  lookup; /* key -> index into nodes */

    std::vector<uint32_t> commands;
    std::vector<uint32_t> events;

    uint32_t epoch;    /* highest accepted */
    int32_t  current;  /* node index the component commands apply to, or -1 */
    bool     in_scene; /* a SCENE_BEGIN is open, possibly from an earlier flush */
    bool     skipping; /* that open scene was stale and is being discarded */
};

namespace {

const int32_t kNoNode = -1;

float word_to_float(uint32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

uint32_t float_to_word(float value) {
    uint32_t word;
    std::memcpy(&word, &value, sizeof word);
    return word;
}

/**
 * Records a payload against a node and reports whether it is new.
 *
 * "New" means the node has not carried this opcode before, or carried it with
 * different words. That distinction is the whole reason a reload can change one
 * value without disturbing the rest of a running simulation.
 */
bool take(SceneNode& node, uint32_t op, uint32_t* stored,
          const uint32_t* payload, uint32_t count) {
    const uint32_t bit = 1u << op;
    bool differs = (node.described & bit) == 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (stored[i] != payload[i]) differs = true;
        stored[i] = payload[i];
    }

    node.described |= bit;
    return differs;
}

EshiSceneState* scene_of(EshiWorld* w) {
    EshiSceneState** slot = eshi__world_scene(w);
    if (!slot) return NULL;
    if (!*slot) *slot = eshi__scene_create();
    return *slot;
}

const EshiSceneState* scene_of_const(const EshiWorld* w) {
    EshiSceneState** slot = eshi__world_scene(const_cast<EshiWorld*>(w));
    return slot ? *slot : NULL;
}

int32_t find_node(const EshiSceneState* scene, uint32_t key) {
    std::map<uint32_t, int32_t>::const_iterator it = scene->lookup.find(key);
    return it == scene->lookup.end() ? kNoNode : it->second;
}

/** Creates the entity for a key not seen before, and retains it. */
int32_t insert_node(EshiWorld* w, EshiSceneState* scene, uint32_t key) {
    const EshiEntity entity = eshi_entity_create(w);
    if (entity == ESHI_NULL_ENTITY) return kNoNode;

    SceneNode node;
    std::memset(&node, 0, sizeof node);
    node.key = key;
    node.entity = entity;
    node.seen = scene->epoch;

    const int32_t index = (int32_t)scene->nodes.size();
    scene->nodes.push_back(node);
    scene->lookup[key] = index;
    return index;
}

/**
 * Destroys every node not described in the current epoch.
 *
 * Descending, because removal is swap-and-pop: the element that fills a vacated
 * slot always comes from the tail, which a descending walk has already visited.
 * Ascending would skip it.
 */
void sweep(EshiWorld* w, EshiSceneState* scene) {
    for (int32_t i = (int32_t)scene->nodes.size() - 1; i >= 0; --i) {
        SceneNode& node = scene->nodes[(size_t)i];
        if (node.seen == scene->epoch) continue;

        eshi_entity_destroy(w, node.entity);
        scene->lookup.erase(node.key);

        const size_t last = scene->nodes.size() - 1;
        if ((size_t)i != last) {
            scene->nodes[(size_t)i] = scene->nodes[last];
            scene->lookup[scene->nodes[(size_t)i].key] = i;
        }
        scene->nodes.pop_back();
    }
}

/** Payload words each opcode requires. A longer payload is legal and ignored. */
uint32_t payload_arity(uint32_t op, bool* known) {
    *known = true;
    switch (op) {
        case ESHI_CMD_NOP:         return 0;
        case ESHI_CMD_SCENE_BEGIN: return 1;
        case ESHI_CMD_NODE:        return 1;
        case ESHI_CMD_TRANSFORM:   return 2;
        case ESHI_CMD_VELOCITY:    return 2;
        case ESHI_CMD_COLLIDER:    return 5;
        case ESHI_CMD_RESTITUTION: return 1;
        case ESHI_CMD_BOUNDS:      return 4;
        case ESHI_CMD_SCENE_END:   return 0;
        case ESHI_CMD_INPUT:       return 2;
        default: break;
    }
    *known = false;
    return 0;
}

/**
 * Applies one scene command to the open node.
 *
 * Returns false only for a malformed command — a component with no node open.
 * A command whose value is unchanged is applied successfully and does nothing,
 * which is the ordinary case on a re-submission.
 */
bool apply_node_command(EshiWorld* w, EshiSceneState* scene,
                        uint32_t op, const uint32_t* p) {
    if (scene->current == kNoNode) return false;
    SceneNode& node = scene->nodes[(size_t)scene->current];

    switch (op) {
        case ESHI_CMD_TRANSFORM:
            if (take(node, op, node.transform, p, 2)) {
                eshi_transform_set(w, node.entity,
                                   word_to_float(p[0]), word_to_float(p[1]));
            }
            return true;

        case ESHI_CMD_VELOCITY:
            if (take(node, op, node.velocity, p, 2)) {
                eshi_velocity_set(w, node.entity,
                                  word_to_float(p[0]), word_to_float(p[1]));
            }
            return true;

        case ESHI_CMD_COLLIDER:
            if (take(node, op, node.collider, p, 5)) {
                eshi_collider_set(w, node.entity,
                                  word_to_float(p[0]), word_to_float(p[1]),
                                  p[2], p[3], p[4]);
            }
            return true;

        case ESHI_CMD_RESTITUTION:
            if (take(node, op, node.restitution, p, 1)) {
                eshi_collider_set_restitution(w, node.entity, word_to_float(p[0]));
            }
            return true;

        case ESHI_CMD_BOUNDS:
            if (take(node, op, node.bounds, p, 4)) {
                eshi_bounds_set(w, node.entity,
                                word_to_float(p[0]), word_to_float(p[1]),
                                word_to_float(p[2]), word_to_float(p[3]));
            }
            return true;

        default:
            return false;
    }
}

EshiResult decode(EshiWorld* w, EshiSceneState* scene,
                  const uint32_t* words, uint32_t word_count,
                  uint32_t* out_applied) {
    EshiResult status = ESHI_OK;
    uint32_t applied = 0;
    uint32_t i = 0;

    while (i < word_count) {
        const uint32_t header = words[i];
        const uint32_t op = ESHI_CMD_OP(header);
        const uint32_t words_declared = ESHI_CMD_WORDS(header);

        /* word_count > i, so the subtraction cannot wrap. */
        if (words_declared > word_count - i - 1) {
            status = ESHI_ERR_INVALID;
            break;
        }

        bool known = false;
        const uint32_t arity = payload_arity(op, &known);
        if (!known) {
            /*
             * The length field bounds-checks and tolerates extra payload; it is
             * not permission to skip a command wholesale. A core that quietly
             * dropped an opcode its Dart package emits would render a scene
             * that is wrong rather than one that is missing, and the mismatch
             * would surface as an art bug weeks later.
             */
            status = ESHI_ERR_UNSUPPORTED;
            break;
        }
        if (words_declared < arity) {
            status = ESHI_ERR_INVALID;
            break;
        }

        const uint32_t* p = words + i + 1;
        bool ok = true;

        switch (op) {
            case ESHI_CMD_NOP:
                break;

            case ESHI_CMD_INPUT:
                /*
                 * Outside the scene vocabulary on purpose, so the epoch gate
                 * does not swallow a frame of input along with a stale scene.
                 */
                if (p[0] >= (uint32_t)ESHI_KEY_COUNT) {
                    ok = false;
                } else {
                    eshi_input_set_key(w, (EshiKey)p[0], p[1] != 0);
                }
                break;

            case ESHI_CMD_SCENE_BEGIN:
                if (scene->in_scene) {
                    ok = false; /* nested scenes have no meaning */
                } else {
                    scene->in_scene = true;
                    scene->current = kNoNode;
                    scene->skipping = p[0] < scene->epoch;
                    if (scene->skipping) {
                        if (status == ESHI_OK) status = ESHI_ERR_STALE;
                    } else {
                        scene->epoch = p[0];
                    }
                }
                break;

            case ESHI_CMD_SCENE_END:
                if (!scene->in_scene) {
                    ok = false;
                } else {
                    if (!scene->skipping) sweep(w, scene);
                    scene->in_scene = false;
                    scene->skipping = false;
                    scene->current = kNoNode;
                }
                break;

            case ESHI_CMD_NODE:
                if (!scene->in_scene || p[0] == 0) {
                    ok = false;
                } else if (scene->skipping) {
                    scene->current = kNoNode;
                } else {
                    int32_t index = find_node(scene, p[0]);
                    if (index == kNoNode) {
                        index = insert_node(w, scene, p[0]);
                        if (index == kNoNode) {
                            status = ESHI_ERR_LIMIT;
                            ok = false;
                        }
                    } else {
                        /* The reload case: the key is already live, so it is
                         * marked and reused rather than created a second time. */
                        scene->nodes[(size_t)index].seen = scene->epoch;
                    }
                    scene->current = index;
                }
                break;

            default:
                if (scene->skipping) break;
                ok = apply_node_command(w, scene, op, p);
                break;
        }

        if (!ok) {
            /* A malformed command outranks a latched ESHI_ERR_STALE, but not a
             * specific failure the case already reported. */
            if (status == ESHI_OK || status == ESHI_ERR_STALE) status = ESHI_ERR_INVALID;
            break;
        }

        i += 1 + words_declared;
        ++applied;
    }

    if (out_applied) *out_applied = applied;
    return status;
}

} /* namespace */

/* ===========================================================================
 * Lifecycle — called by the world, which owns the one instance
 * ==========================================================================*/
extern "C" EshiSceneState* eshi__scene_create(void) {
    EshiSceneState* scene = new (std::nothrow) EshiSceneState();
    if (!scene) return NULL;
    scene->epoch = 0;
    scene->current = kNoNode;
    scene->in_scene = false;
    scene->skipping = false;
    return scene;
}

extern "C" void eshi__scene_destroy(EshiSceneState* scene) { delete scene; }

/* ===========================================================================
 * Command buffer
 * ==========================================================================*/
extern "C" EshiResult eshi_commands_reserve(EshiWorld* w, uint32_t words) {
    if (!w) return ESHI_ERR_INVALID;
    EshiSceneState* scene = scene_of(w);
    if (!scene) return ESHI_ERR_NOMEM;
    if (scene->commands.size() >= words) return ESHI_OK;

    scene->commands.resize(words, 0u);
    return ESHI_OK;
}

extern "C" uint32_t* eshi_commands_data(EshiWorld* w) {
    if (!w) return NULL;
    EshiSceneState* scene = scene_of(w);
    if (!scene || scene->commands.empty()) return NULL;
    return &scene->commands[0];
}

extern "C" uint32_t eshi_commands_capacity(const EshiWorld* w) {
    if (!w) return 0;
    const EshiSceneState* scene = scene_of_const(w);
    return scene ? (uint32_t)scene->commands.size() : 0;
}

extern "C" EshiResult eshi_commands_submit(EshiWorld* w, const uint32_t* words,
                                           uint32_t word_count, uint32_t* out_applied) {
    if (out_applied) *out_applied = 0;
    if (!w) return ESHI_ERR_INVALID;
    if (word_count == 0) return ESHI_OK;
    if (!words) return ESHI_ERR_INVALID;

    EshiSceneState* scene = scene_of(w);
    if (!scene) return ESHI_ERR_NOMEM;
    return decode(w, scene, words, word_count, out_applied);
}

extern "C" EshiResult eshi_commands_flush(EshiWorld* w, uint32_t word_count,
                                          uint32_t* out_applied) {
    if (out_applied) *out_applied = 0;
    if (!w) return ESHI_ERR_INVALID;

    EshiSceneState* scene = scene_of(w);
    if (!scene) return ESHI_ERR_NOMEM;
    if (word_count > scene->commands.size()) return ESHI_ERR_LIMIT;
    if (word_count == 0) return ESHI_OK;

    return decode(w, scene, &scene->commands[0], word_count, out_applied);
}

/* ===========================================================================
 * Scene read-back
 * ==========================================================================*/
extern "C" EshiEntity eshi_scene_entity(const EshiWorld* w, uint32_t key) {
    if (!w || key == 0) return ESHI_NULL_ENTITY;
    const EshiSceneState* scene = scene_of_const(w);
    if (!scene) return ESHI_NULL_ENTITY;

    const int32_t index = find_node(scene, key);
    return index == kNoNode ? ESHI_NULL_ENTITY : scene->nodes[(size_t)index].entity;
}

extern "C" uint32_t eshi_scene_epoch(const EshiWorld* w) {
    if (!w) return 0;
    const EshiSceneState* scene = scene_of_const(w);
    return scene ? scene->epoch : 0;
}

extern "C" uint32_t eshi_scene_node_count(const EshiWorld* w) {
    if (!w) return 0;
    const EshiSceneState* scene = scene_of_const(w);
    return scene ? (uint32_t)scene->nodes.size() : 0;
}

extern "C" void eshi_scene_clear(EshiWorld* w) {
    if (!w) return;
    EshiSceneState** slot = eshi__world_scene(w);
    EshiSceneState* scene = slot ? *slot : NULL;
    if (!scene) return;

    for (size_t i = 0; i < scene->nodes.size(); ++i) {
        eshi_entity_destroy(w, scene->nodes[i].entity);
    }
    scene->nodes.clear();
    scene->lookup.clear();
    scene->current = kNoNode;
    scene->in_scene = false;
    scene->skipping = false;
}

/* ===========================================================================
 * Event buffer
 * ==========================================================================*/
extern "C" EshiResult eshi_events_reserve(EshiWorld* w, uint32_t words) {
    if (!w) return ESHI_ERR_INVALID;
    EshiSceneState* scene = scene_of(w);
    if (!scene) return ESHI_ERR_NOMEM;
    if (scene->events.size() < words) scene->events.resize(words, 0u);
    return ESHI_OK;
}

extern "C" const uint32_t* eshi_events_data(const EshiWorld* w) {
    if (!w) return NULL;
    const EshiSceneState* scene = scene_of_const(w);
    if (!scene || scene->events.empty()) return NULL;
    return &scene->events[0];
}

extern "C" uint32_t eshi_events_capacity(const EshiWorld* w) {
    if (!w) return 0;
    const EshiSceneState* scene = scene_of_const(w);
    return scene ? (uint32_t)scene->events.size() : 0;
}

extern "C" EshiResult eshi_events_pack(EshiWorld* w, uint32_t* out_words) {
    if (out_words) *out_words = 0;
    if (!w) return ESHI_ERR_INVALID;

    EshiSceneState* scene = scene_of(w);
    if (!scene) return ESHI_ERR_NOMEM;

    int32_t count = 0;
    const EshiCollisionEvent* events = eshi__collisions(w, &count);

    const uint32_t kRecordWords = 6; /* header + a, b, nx, ny, penetration */
    const uint32_t capacity = (uint32_t)scene->events.size();
    uint32_t written = 0;

    for (int32_t i = 0; i < count; ++i) {
        if (written + kRecordWords > capacity) {
            if (out_words) *out_words = written;
            return ESHI_ERR_LIMIT;
        }

        uint32_t* out = &scene->events[written];
        out[0] = ESHI_CMD_HEADER(ESHI_EVENT_COLLISION, kRecordWords - 1);
        out[1] = (uint32_t)events[i].a;
        out[2] = (uint32_t)events[i].b;
        out[3] = float_to_word(events[i].nx);
        out[4] = float_to_word(events[i].ny);
        out[5] = float_to_word(events[i].penetration);
        written += kRecordWords;
    }

    if (out_words) *out_words = written;
    return ESHI_OK;
}
