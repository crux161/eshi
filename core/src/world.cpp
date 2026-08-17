/**
 * @file world.cpp
 * @brief Larimar core — entity storage, systems, motion, collision, input.
 *
 * Storage is a sparse set per component type: dense parallel arrays for the
 * data, plus an index table mapping entity -> dense slot. Systems walk the
 * dense arrays, so iteration touches only live data and stays contiguous.
 *
 * Nothing in this file includes a platform header. That is load-bearing, not
 * incidental — see docs/larimar/ARCHITECTURE.md §5.
 */
#include "eshi/eshi.h"
#include "eshi_internal.h"

#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

const int32_t kNoSlot = -1;

/* ---------------------------------------------------------------------------
 * Sparse set index. Shared by every component array.
 * -------------------------------------------------------------------------*/
struct SparseIndex {
    std::vector<int32_t>    entity_to_slot; /* indexed by entity index */
    std::vector<EshiEntity> slot_to_entity; /* dense */

    void reserve(uint32_t max_entities) {
        entity_to_slot.assign(max_entities + 1, kNoSlot);
    }

    int32_t slot_of(EshiEntity e) const {
        const uint32_t idx = ESHI_ENTITY_INDEX(e);
        if (idx >= entity_to_slot.size()) return kNoSlot;
        return entity_to_slot[idx];
    }

    bool has(EshiEntity e) const { return slot_of(e) != kNoSlot; }

    /* Returns the slot for e, creating one if absent. */
    int32_t insert(EshiEntity e) {
        const int32_t existing = slot_of(e);
        if (existing != kNoSlot) return existing;

        const int32_t slot = (int32_t)slot_to_entity.size();
        slot_to_entity.push_back(e);
        entity_to_slot[ESHI_ENTITY_INDEX(e)] = slot;
        return slot;
    }

    /*
     * Swap-and-pop. Returns the slot that was vacated and the slot that moved
     * into it, so the caller can mirror the move in its parallel data arrays.
     * moved_from == -1 means the removed element was already last.
     */
    bool remove(EshiEntity e, int32_t* out_removed_slot, int32_t* out_moved_from) {
        const int32_t slot = slot_of(e);
        if (slot == kNoSlot) return false;

        const int32_t last = (int32_t)slot_to_entity.size() - 1;
        entity_to_slot[ESHI_ENTITY_INDEX(e)] = kNoSlot;

        if (slot != last) {
            const EshiEntity moved = slot_to_entity[last];
            slot_to_entity[slot] = moved;
            entity_to_slot[ESHI_ENTITY_INDEX(moved)] = slot;
            *out_moved_from = last;
        } else {
            *out_moved_from = kNoSlot;
        }

        slot_to_entity.pop_back();
        *out_removed_slot = slot;
        return true;
    }

    int32_t count() const { return (int32_t)slot_to_entity.size(); }
};

/* Mirrors a swap-and-pop into a parallel data array. */
template <typename T>
void apply_removal(std::vector<T>& data, int32_t removed_slot, int32_t moved_from) {
    if (moved_from != kNoSlot) data[removed_slot] = data[moved_from];
    data.pop_back();
}

/* ---------------------------------------------------------------------------
 * Component arrays
 * -------------------------------------------------------------------------*/
struct Transforms {
    SparseIndex        index;
    std::vector<float> x, y;
};

struct Velocities {
    SparseIndex        index;
    std::vector<float> vx, vy;
};

struct Colliders {
    SparseIndex           index;
    std::vector<float>    hx, hy;
    std::vector<uint32_t> layer, mask, flags;
    std::vector<float>    restitution;
};

struct BoundsClamps {
    SparseIndex        index;
    std::vector<float> min_x, max_x, min_y, max_y;
};

struct SystemEntry {
    std::string  name;
    int32_t      order;
    uint32_t     sequence; /* stable tie-break, so ordering never depends on sort stability */
    EshiSystemFn fn;
    void*        user;
};

} /* namespace */

/* ---------------------------------------------------------------------------
 * The world
 * -------------------------------------------------------------------------*/
struct EshiWorld {
    EshiConfig cfg;

    /* entity allocator */
    std::vector<uint8_t>    generation;  /* indexed by entity index */
    std::vector<uint32_t>   free_indices;
    uint32_t                next_index;
    uint32_t                live_count;

    Transforms   transforms;
    Velocities   velocities;
    Colliders    colliders;
    BoundsClamps bounds;

    std::vector<SystemEntry>        systems;
    uint32_t                        system_sequence;
    std::vector<EshiCollisionEvent> collisions;

    uint8_t keys[ESHI_KEY_COUNT];
    uint8_t keys_prev[ESHI_KEY_COUNT];

    EshiMaterial material;

    const EshiBackendVTable* backend_vtable;
    EshiBackend*             backend;

    float    accumulator;
    uint64_t frame;
    double   sim_time;
    uint64_t rng_state;
};

namespace {

/* Rejects a handle whose generation no longer matches its slot. */
bool alive(const EshiWorld* w, EshiEntity e) {
    if (e == ESHI_NULL_ENTITY) return false;
    const uint32_t idx = ESHI_ENTITY_INDEX(e);
    if (idx >= w->generation.size()) return false;
    return w->generation[idx] == (uint8_t)ESHI_ENTITY_GENERATION(e);
}

EshiEntity make_entity(uint32_t index, uint8_t generation) {
    return (EshiEntity)((uint32_t)generation << ESHI_ENTITY_INDEX_BITS |
                        (index & ESHI_MAX_ENTITIES));
}

/* splitmix64 — small, seedable, and reproducible across platforms. */
uint64_t next_random(EshiWorld* w) {
    uint64_t z = (w->rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

/* --------------------------------------------------------------------------
 * Built-in systems
 * ------------------------------------------------------------------------*/

void system_motion(EshiWorld* w, float dt, void* /*user*/) {
    /*
     * Velocity and transform are separate sparse sets, so a moving entity's
     * slots differ between them. Walk the smaller (velocity) set and look up
     * the transform slot once per entity.
     */
    const int32_t count = w->velocities.index.count();
    for (int32_t i = 0; i < count; ++i) {
        const EshiEntity e = w->velocities.index.slot_to_entity[i];
        const int32_t t = w->transforms.index.slot_of(e);
        if (t == kNoSlot) continue;
        w->transforms.x[t] += w->velocities.vx[i] * dt;
        w->transforms.y[t] += w->velocities.vy[i] * dt;
    }
}

void system_bounds(EshiWorld* w, float /*dt*/, void* /*user*/) {
    const int32_t count = w->bounds.index.count();
    for (int32_t i = 0; i < count; ++i) {
        const EshiEntity e = w->bounds.index.slot_to_entity[i];
        const int32_t t = w->transforms.index.slot_of(e);
        if (t == kNoSlot) continue;

        float& x = w->transforms.x[t];
        float& y = w->transforms.y[t];
        if (x < w->bounds.min_x[i]) x = w->bounds.min_x[i];
        if (x > w->bounds.max_x[i]) x = w->bounds.max_x[i];
        if (y < w->bounds.min_y[i]) y = w->bounds.min_y[i];
        if (y > w->bounds.max_y[i]) y = w->bounds.max_y[i];
    }
}

/*
 * Brute-force pairwise AABB. O(n^2) is correct and is not the bottleneck at
 * Pong's entity counts; a broadphase belongs here when a scene needs it, and
 * swapping one in does not change the event contract above it.
 */
void system_collision(EshiWorld* w, float /*dt*/, void* /*user*/) {
    w->collisions.clear();

    const int32_t count = w->colliders.index.count();
    for (int32_t i = 0; i < count; ++i) {
        const EshiEntity ea = w->colliders.index.slot_to_entity[i];
        const int32_t ta = w->transforms.index.slot_of(ea);
        if (ta == kNoSlot) continue;

        for (int32_t j = i + 1; j < count; ++j) {
            const EshiEntity eb = w->colliders.index.slot_to_entity[j];
            const int32_t tb = w->transforms.index.slot_of(eb);
            if (tb == kNoSlot) continue;

            const bool a_sees_b = (w->colliders.layer[i] & w->colliders.mask[j]) != 0;
            const bool b_sees_a = (w->colliders.layer[j] & w->colliders.mask[i]) != 0;
            if (!a_sees_b || !b_sees_a) continue;

            const float dx = w->transforms.x[ta] - w->transforms.x[tb];
            const float dy = w->transforms.y[ta] - w->transforms.y[tb];
            const float ox = (w->colliders.hx[i] + w->colliders.hx[j]) - std::fabs(dx);
            const float oy = (w->colliders.hy[i] + w->colliders.hy[j]) - std::fabs(dy);
            if (ox <= 0.0f || oy <= 0.0f) continue;

            /* Resolve along the axis of least penetration. */
            float nx = 0.0f, ny = 0.0f, penetration;
            if (ox < oy) {
                nx = (dx < 0.0f) ? -1.0f : 1.0f;
                penetration = ox;
            } else {
                ny = (dy < 0.0f) ? -1.0f : 1.0f;
                penetration = oy;
            }

            EshiCollisionEvent ev;
            ev.a = ea;
            ev.b = eb;
            ev.nx = nx;
            ev.ny = ny;
            ev.penetration = penetration;
            w->collisions.push_back(ev);

            const bool trigger = ((w->colliders.flags[i] | w->colliders.flags[j]) &
                                  ESHI_COLLIDER_TRIGGER) != 0;
            if (trigger) continue;

            const bool a_static = (w->colliders.flags[i] & ESHI_COLLIDER_STATIC) != 0;
            const bool b_static = (w->colliders.flags[j] & ESHI_COLLIDER_STATIC) != 0;
            if (a_static && b_static) continue;

            /* Positional correction, split between whichever bodies can move. */
            const float share = (a_static || b_static) ? 1.0f : 0.5f;
            if (!a_static) {
                w->transforms.x[ta] += nx * penetration * share;
                w->transforms.y[ta] += ny * penetration * share;
            }
            if (!b_static) {
                w->transforms.x[tb] -= nx * penetration * share;
                w->transforms.y[tb] -= ny * penetration * share;
            }

            /* Reflect velocity about the contact normal, scaled by restitution. */
            const float restitution =
                (w->colliders.restitution[i] < w->colliders.restitution[j])
                    ? w->colliders.restitution[i]
                    : w->colliders.restitution[j];

            const int32_t va = w->velocities.index.slot_of(ea);
            if (va != kNoSlot && !a_static) {
                const float vn = w->velocities.vx[va] * nx + w->velocities.vy[va] * ny;
                if (vn < 0.0f) {
                    w->velocities.vx[va] -= (1.0f + restitution) * vn * nx;
                    w->velocities.vy[va] -= (1.0f + restitution) * vn * ny;
                }
            }
            const int32_t vb = w->velocities.index.slot_of(eb);
            if (vb != kNoSlot && !b_static) {
                const float vn = -(w->velocities.vx[vb] * nx + w->velocities.vy[vb] * ny);
                if (vn < 0.0f) {
                    w->velocities.vx[vb] += (1.0f + restitution) * vn * nx;
                    w->velocities.vy[vb] += (1.0f + restitution) * vn * ny;
                }
            }
        }
    }
}

void insert_system(EshiWorld* w, const char* name, int32_t order,
                   EshiSystemFn fn, void* user) {
    SystemEntry entry;
    entry.name = name ? name : "";
    entry.order = order;
    entry.sequence = w->system_sequence++;
    entry.fn = fn;
    entry.user = user;

    /*
     * Insertion sort on (order, sequence). Registration is rare and the list is
     * short, and this keeps execution order independent of any library sort's
     * stability guarantees — which the deterministic export depends on.
     */
    size_t pos = w->systems.size();
    while (pos > 0) {
        const SystemEntry& prev = w->systems[pos - 1];
        if (prev.order < entry.order ||
            (prev.order == entry.order && prev.sequence < entry.sequence)) {
            break;
        }
        --pos;
    }
    w->systems.insert(w->systems.begin() + (ptrdiff_t)pos, entry);
}

} /* namespace */

/* ===========================================================================
 * Diagnostics
 * ==========================================================================*/
extern "C" const char* eshi_result_string(EshiResult r) {
    switch (r) {
        case ESHI_OK:              return "ok";
        case ESHI_ERR_INVALID:     return "invalid argument or dead entity";
        case ESHI_ERR_UNSUPPORTED: return "unsupported grade or feature";
        case ESHI_ERR_NOMEM:       return "out of memory";
        case ESHI_ERR_LIMIT:       return "capacity exhausted";
    }
    return "unknown";
}

extern "C" const char* eshi_grade_string(EshiGrade g) {
    switch (g) {
        case ESHI_GRADE_INK:   return "ink";
        case ESHI_GRADE_PAPER: return "paper";
        case ESHI_GRADE_BRUSH: return "brush";
        case ESHI_GRADE_GOLD:  return "gold";
    }
    return "unknown";
}

/* ===========================================================================
 * Lifecycle
 * ==========================================================================*/
extern "C" EshiConfig eshi_config_default(void) {
    EshiConfig cfg;
    cfg.width = 960;
    cfg.height = 540;
    cfg.grade = ESHI_GRADE_INK;
    cfg.fixed_dt = 1.0f / 60.0f;
    cfg.seed = 0x5EED5EEDull;
    cfg.max_entities = 4096;
    return cfg;
}

extern "C" EshiWorld* eshi_world_create(const EshiConfig* in_cfg) {
    EshiConfig cfg = in_cfg ? *in_cfg : eshi_config_default();
    if (cfg.width <= 0 || cfg.height <= 0) return NULL;
    if (cfg.fixed_dt <= 0.0f) cfg.fixed_dt = 1.0f / 60.0f;
    if (cfg.max_entities == 0) cfg.max_entities = 4096;
    if (cfg.max_entities > ESHI_MAX_ENTITIES) return NULL;

    /*
     * A grade must be genuinely available. Failing here beats silently
     * downgrading a caller who asked for hardware they expected to have — a
     * host that wants a fallback probes with eshi_grade_available() first.
     */
    const EshiBackendVTable* vtable = eshi__backend_for_grade(cfg.grade);
    if (!vtable || !vtable->available()) return NULL;

    EshiWorld* w = new (std::nothrow) EshiWorld();
    if (!w) return NULL;

    w->backend_vtable = vtable;
    w->backend = NULL;

    w->cfg = cfg;
    w->generation.assign(cfg.max_entities + 1, 0);
    w->next_index = 1; /* index 0 is reserved so ESHI_NULL_ENTITY stays invalid */
    w->live_count = 0;

    w->transforms.index.reserve(cfg.max_entities);
    w->velocities.index.reserve(cfg.max_entities);
    w->colliders.index.reserve(cfg.max_entities);
    w->bounds.index.reserve(cfg.max_entities);

    w->system_sequence = 0;
    std::memset(w->keys, 0, sizeof(w->keys));
    std::memset(w->keys_prev, 0, sizeof(w->keys_prev));

    std::memset(&w->material, 0, sizeof(w->material));
    w->accumulator = 0.0f;
    w->frame = 0;
    w->sim_time = 0.0;
    w->rng_state = cfg.seed ? cfg.seed : 0x5EED5EEDull;

    insert_system(w, "motion",    ESHI_ORDER_MOTION,        system_motion,    NULL);
    insert_system(w, "bounds",    ESHI_ORDER_MOTION + 1,    system_bounds,    NULL);
    insert_system(w, "collision", ESHI_ORDER_COLLISION,     system_collision, NULL);

    return w;
}

extern "C" void eshi_world_destroy(EshiWorld* w) {
    if (!w) return;
    if (w->backend && w->backend_vtable) w->backend_vtable->destroy(w->backend);
    delete w;
}

extern "C" EshiGrade eshi_world_grade(const EshiWorld* w) {
    return w ? w->cfg.grade : ESHI_GRADE_INK;
}

extern "C" const char* eshi_world_backend_name(const EshiWorld* w) {
    if (!w || !w->backend_vtable || !w->backend_vtable->name) return "unavailable";
    return w->backend_vtable->name;
}

extern "C" void eshi_world_size(const EshiWorld* w, int32_t* out_w, int32_t* out_h) {
    if (!w) return;
    if (out_w) *out_w = w->cfg.width;
    if (out_h) *out_h = w->cfg.height;
}

/* ===========================================================================
 * Entities
 * ==========================================================================*/
extern "C" EshiEntity eshi_entity_create(EshiWorld* w) {
    if (!w) return ESHI_NULL_ENTITY;

    uint32_t index;
    if (!w->free_indices.empty()) {
        index = w->free_indices.back();
        w->free_indices.pop_back();
    } else {
        if (w->next_index > w->cfg.max_entities) return ESHI_NULL_ENTITY;
        index = w->next_index++;
    }

    w->live_count++;
    return make_entity(index, w->generation[index]);
}

extern "C" void eshi_entity_destroy(EshiWorld* w, EshiEntity e) {
    if (!w || !alive(w, e)) return;

    int32_t removed, moved;
    if (w->transforms.index.remove(e, &removed, &moved)) {
        apply_removal(w->transforms.x, removed, moved);
        apply_removal(w->transforms.y, removed, moved);
    }
    if (w->velocities.index.remove(e, &removed, &moved)) {
        apply_removal(w->velocities.vx, removed, moved);
        apply_removal(w->velocities.vy, removed, moved);
    }
    if (w->colliders.index.remove(e, &removed, &moved)) {
        apply_removal(w->colliders.hx, removed, moved);
        apply_removal(w->colliders.hy, removed, moved);
        apply_removal(w->colliders.layer, removed, moved);
        apply_removal(w->colliders.mask, removed, moved);
        apply_removal(w->colliders.flags, removed, moved);
        apply_removal(w->colliders.restitution, removed, moved);
    }
    if (w->bounds.index.remove(e, &removed, &moved)) {
        apply_removal(w->bounds.min_x, removed, moved);
        apply_removal(w->bounds.max_x, removed, moved);
        apply_removal(w->bounds.min_y, removed, moved);
        apply_removal(w->bounds.max_y, removed, moved);
    }

    const uint32_t index = ESHI_ENTITY_INDEX(e);
    w->generation[index] = (uint8_t)((w->generation[index] + 1) & 0xFFu);
    w->free_indices.push_back(index);
    w->live_count--;
}

extern "C" int eshi_entity_alive(const EshiWorld* w, EshiEntity e) {
    return (w && alive(w, e)) ? 1 : 0;
}

extern "C" uint32_t eshi_entity_count(const EshiWorld* w) {
    return w ? w->live_count : 0;
}

/* ===========================================================================
 * Components
 * ==========================================================================*/
extern "C" EshiResult eshi_transform_set(EshiWorld* w, EshiEntity e, float x, float y) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->transforms.index.insert(e);
    if (slot == (int32_t)w->transforms.x.size()) {
        w->transforms.x.push_back(x);
        w->transforms.y.push_back(y);
    } else {
        w->transforms.x[slot] = x;
        w->transforms.y[slot] = y;
    }
    return ESHI_OK;
}

extern "C" EshiResult eshi_transform_get(const EshiWorld* w, EshiEntity e, float* x, float* y) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->transforms.index.slot_of(e);
    if (slot == kNoSlot) return ESHI_ERR_INVALID;
    if (x) *x = w->transforms.x[slot];
    if (y) *y = w->transforms.y[slot];
    return ESHI_OK;
}

extern "C" EshiResult eshi_velocity_set(EshiWorld* w, EshiEntity e, float vx, float vy) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->velocities.index.insert(e);
    if (slot == (int32_t)w->velocities.vx.size()) {
        w->velocities.vx.push_back(vx);
        w->velocities.vy.push_back(vy);
    } else {
        w->velocities.vx[slot] = vx;
        w->velocities.vy[slot] = vy;
    }
    return ESHI_OK;
}

extern "C" EshiResult eshi_velocity_get(const EshiWorld* w, EshiEntity e, float* vx, float* vy) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->velocities.index.slot_of(e);
    if (slot == kNoSlot) return ESHI_ERR_INVALID;
    if (vx) *vx = w->velocities.vx[slot];
    if (vy) *vy = w->velocities.vy[slot];
    return ESHI_OK;
}

extern "C" EshiResult eshi_collider_set(EshiWorld* w, EshiEntity e,
                                        float hx, float hy,
                                        uint32_t layer, uint32_t mask,
                                        uint32_t flags) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->colliders.index.insert(e);
    if (slot == (int32_t)w->colliders.hx.size()) {
        w->colliders.hx.push_back(hx);
        w->colliders.hy.push_back(hy);
        w->colliders.layer.push_back(layer);
        w->colliders.mask.push_back(mask);
        w->colliders.flags.push_back(flags);
        w->colliders.restitution.push_back(1.0f);
    } else {
        w->colliders.hx[slot] = hx;
        w->colliders.hy[slot] = hy;
        w->colliders.layer[slot] = layer;
        w->colliders.mask[slot] = mask;
        w->colliders.flags[slot] = flags;
    }
    return ESHI_OK;
}

extern "C" EshiResult eshi_collider_set_restitution(EshiWorld* w, EshiEntity e, float restitution) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->colliders.index.slot_of(e);
    if (slot == kNoSlot) return ESHI_ERR_INVALID;
    w->colliders.restitution[slot] = restitution;
    return ESHI_OK;
}

extern "C" EshiResult eshi_bounds_set(EshiWorld* w, EshiEntity e,
                                      float min_x, float max_x,
                                      float min_y, float max_y) {
    if (!w || !alive(w, e)) return ESHI_ERR_INVALID;
    const int32_t slot = w->bounds.index.insert(e);
    if (slot == (int32_t)w->bounds.min_x.size()) {
        w->bounds.min_x.push_back(min_x);
        w->bounds.max_x.push_back(max_x);
        w->bounds.min_y.push_back(min_y);
        w->bounds.max_y.push_back(max_y);
    } else {
        w->bounds.min_x[slot] = min_x;
        w->bounds.max_x[slot] = max_x;
        w->bounds.min_y[slot] = min_y;
        w->bounds.max_y[slot] = max_y;
    }
    return ESHI_OK;
}

/* ===========================================================================
 * Views
 * ==========================================================================*/
extern "C" EshiTransformView eshi_view_transforms(EshiWorld* w) {
    EshiTransformView v;
    if (!w || w->transforms.index.count() == 0) {
        v.entity = NULL; v.x = NULL; v.y = NULL; v.count = 0;
        return v;
    }
    v.entity = &w->transforms.index.slot_to_entity[0];
    v.x = &w->transforms.x[0];
    v.y = &w->transforms.y[0];
    v.count = w->transforms.index.count();
    return v;
}

extern "C" EshiVelocityView eshi_view_velocities(EshiWorld* w) {
    EshiVelocityView v;
    if (!w || w->velocities.index.count() == 0) {
        v.entity = NULL; v.vx = NULL; v.vy = NULL; v.count = 0;
        return v;
    }
    v.entity = &w->velocities.index.slot_to_entity[0];
    v.vx = &w->velocities.vx[0];
    v.vy = &w->velocities.vy[0];
    v.count = w->velocities.index.count();
    return v;
}

/* ===========================================================================
 * Systems and events
 * ==========================================================================*/
extern "C" EshiResult eshi_system_add(EshiWorld* w, const char* name, int32_t order,
                                      EshiSystemFn fn, void* user) {
    if (!w || !fn) return ESHI_ERR_INVALID;
    insert_system(w, name, order, fn, user);
    return ESHI_OK;
}

extern "C" int32_t eshi_collisions_poll(EshiWorld* w, EshiCollisionEvent* out, int32_t max) {
    if (!w || !out || max <= 0) return 0;
    const int32_t n = (int32_t)w->collisions.size() < max ? (int32_t)w->collisions.size() : max;
    for (int32_t i = 0; i < n; ++i) out[i] = w->collisions[(size_t)i];
    return n;
}

/* ===========================================================================
 * Input
 * ==========================================================================*/
extern "C" void eshi_input_set_key(EshiWorld* w, EshiKey key, int down) {
    if (!w || key < 0 || key >= ESHI_KEY_COUNT) return;
    w->keys[key] = down ? 1 : 0;
}

extern "C" int eshi_input_down(const EshiWorld* w, EshiKey key) {
    if (!w || key < 0 || key >= ESHI_KEY_COUNT) return 0;
    return w->keys[key] ? 1 : 0;
}

extern "C" int eshi_input_pressed(const EshiWorld* w, EshiKey key) {
    if (!w || key < 0 || key >= ESHI_KEY_COUNT) return 0;
    return (w->keys[key] && !w->keys_prev[key]) ? 1 : 0;
}

extern "C" int eshi_input_released(const EshiWorld* w, EshiKey key) {
    if (!w || key < 0 || key >= ESHI_KEY_COUNT) return 0;
    return (!w->keys[key] && w->keys_prev[key]) ? 1 : 0;
}

/* ===========================================================================
 * Material
 * ==========================================================================*/
extern "C" EshiResult eshi_material_set(EshiWorld* w, const EshiMaterial* material) {
    if (!w || !material) return ESHI_ERR_INVALID;
    if (!material->cpu_shader && !material->source_path && !material->package_path) {
        return ESHI_ERR_INVALID;
    }

    /*
     * The backend is built here rather than at world creation because renderers
     * only learn the material's source or package path now. Rebinding therefore
     * recompiles source-backed tiers or reloads a rebuilt package, which is the
     * material hot-reload seam. Ink remains compiled into the binary.
     */
    if (w->backend) {
        w->backend_vtable->destroy(w->backend);
        w->backend = NULL;
    }

    EshiBackend* backend =
        w->backend_vtable->create(w->cfg.width, w->cfg.height,
                                  material->source_path, material->package_path);
    if (!backend) return ESHI_ERR_UNSUPPORTED;

    w->backend = backend;
    w->material = *material;
    return ESHI_OK;
}

extern "C" EshiResult eshi_render(EshiWorld* w, uint8_t* pixels, int32_t stride, float time) {
    if (!w || !pixels || stride <= 0) return ESHI_ERR_INVALID;
    if (!w->backend) return ESHI_ERR_INVALID;
    return w->backend_vtable->render(w->backend, pixels, stride, time,
                                     w->material.cpu_shader,
                                     w->material.uniform_data,
                                     w->material.uniform_size);
}

/* ===========================================================================
 * Frame
 * ==========================================================================*/
extern "C" void eshi_tick(EshiWorld* w, float dt) {
    if (!w) return;

    w->accumulator += dt;

    /*
     * Clamp the number of catch-up steps. A stalled host must not be able to
     * turn one late frame into an unbounded simulation burst.
     */
    int steps = 0;
    const int kMaxSteps = 8;
    while (w->accumulator >= w->cfg.fixed_dt && steps < kMaxSteps) {
        std::memcpy(w->keys_prev, w->keys, sizeof(w->keys));

        for (size_t i = 0; i < w->systems.size(); ++i) {
            w->systems[i].fn(w, w->cfg.fixed_dt, w->systems[i].user);
        }

        w->accumulator -= w->cfg.fixed_dt;
        w->sim_time += (double)w->cfg.fixed_dt;
        w->frame++;
        steps++;
    }

    if (steps == kMaxSteps) w->accumulator = 0.0f;
}

extern "C" uint64_t eshi_frame_index(const EshiWorld* w) { return w ? w->frame : 0; }
extern "C" double   eshi_sim_time(const EshiWorld* w)    { return w ? w->sim_time : 0.0; }

extern "C" void eshi__frame_params(EshiWorld* w,
                                   EshiShaderFn* out_shader,
                                   const void**  out_uniforms,
                                   size_t*       out_uniform_size,
                                   const char**  out_source_path,
                                   int32_t*      out_width,
                                   int32_t*      out_height) {
    if (!w) return;
    if (out_shader)       *out_shader = w->material.cpu_shader;
    if (out_uniforms)     *out_uniforms = w->material.uniform_data;
    if (out_uniform_size) *out_uniform_size = w->material.uniform_size;
    if (out_source_path)  *out_source_path = w->material.source_path;
    if (out_width)        *out_width = w->cfg.width;
    if (out_height)       *out_height = w->cfg.height;
}

/* ===========================================================================
 * RNG
 * ==========================================================================*/
extern "C" uint32_t eshi_random_u32(EshiWorld* w) {
    return w ? (uint32_t)(next_random(w) >> 32) : 0;
}

extern "C" float eshi_random_float(EshiWorld* w) {
    /* 24 bits of mantissa is the most a float can carry without rounding to 1. */
    return w ? (float)(eshi_random_u32(w) >> 8) * (1.0f / 16777216.0f) : 0.0f;
}

extern "C" float eshi_random_range(EshiWorld* w, float lo, float hi) {
    return lo + (hi - lo) * eshi_random_float(w);
}
