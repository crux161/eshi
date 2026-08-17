/**
 * @file scene.hpp
 * @brief C++ authoring layer over the command wire format.
 *
 * Same relationship shader.hpp has to the material ABI: the boundary stays C,
 * but nothing should have to hand-pack a header word to describe a paddle.
 *
 * This is also the executable specification of the format. When the Dart
 * package is written in Phase 3 it emits these same words from the same
 * ordering rules, so a disagreement between the two encoders shows up as a
 * failing core test rather than as a garbled scene.
 *
 * No allocation, no exceptions, no dependency beyond eshi.h. A writer that runs
 * out of room latches `overflowed()` and stops emitting rather than truncating
 * mid-command — a half-written command is the one thing the decoder cannot be
 * expected to survive sensibly.
 */
#ifndef ESHI_SCENE_HPP
#define ESHI_SCENE_HPP

#include <cstring>

#include "eshi.h"

namespace eshi {

/**
 * Packs commands into caller-provided words.
 *
 * Usage:
 * @code
 *   uint32_t words[256];
 *   eshi::SceneWriter scene(words, 256);
 *   scene.begin(epoch)
 *        .node(kBall).transform(0.0f, 0.0f).collider(r, r, kBall, kWall, 0)
 *        .end();
 *   scene.submit(world);
 * @endcode
 */
class SceneWriter {
  public:
    SceneWriter(uint32_t* words, uint32_t capacity)
        : words_(words), capacity_(capacity), size_(0), overflowed_(false) {}

    SceneWriter& nop() { return emit(ESHI_CMD_NOP, NULL, 0); }

    SceneWriter& begin(uint32_t epoch) {
        uint32_t p[1] = { epoch };
        return emit(ESHI_CMD_SCENE_BEGIN, p, 1);
    }

    SceneWriter& node(uint32_t key) {
        uint32_t p[1] = { key };
        return emit(ESHI_CMD_NODE, p, 1);
    }

    SceneWriter& transform(float x, float y) {
        uint32_t p[2] = { word(x), word(y) };
        return emit(ESHI_CMD_TRANSFORM, p, 2);
    }

    SceneWriter& velocity(float vx, float vy) {
        uint32_t p[2] = { word(vx), word(vy) };
        return emit(ESHI_CMD_VELOCITY, p, 2);
    }

    SceneWriter& collider(float hx, float hy,
                          uint32_t layer, uint32_t mask, uint32_t flags) {
        uint32_t p[5] = { word(hx), word(hy), layer, mask, flags };
        return emit(ESHI_CMD_COLLIDER, p, 5);
    }

    SceneWriter& restitution(float value) {
        uint32_t p[1] = { word(value) };
        return emit(ESHI_CMD_RESTITUTION, p, 1);
    }

    SceneWriter& bounds(float min_x, float max_x, float min_y, float max_y) {
        uint32_t p[4] = { word(min_x), word(max_x), word(min_y), word(max_y) };
        return emit(ESHI_CMD_BOUNDS, p, 4);
    }

    SceneWriter& input(EshiKey key, bool down) {
        uint32_t p[2] = { (uint32_t)key, down ? 1u : 0u };
        return emit(ESHI_CMD_INPUT, p, 2);
    }

    SceneWriter& end() { return emit(ESHI_CMD_SCENE_END, NULL, 0); }

    uint32_t size() const { return size_; }
    bool     overflowed() const { return overflowed_; }

    /** Hands the buffer to the decoder, or reports the overflow instead. */
    EshiResult submit(EshiWorld* w, uint32_t* out_applied = NULL) const {
        if (overflowed_) return ESHI_ERR_LIMIT;
        return eshi_commands_submit(w, words_, size_, out_applied);
    }

  private:
    static uint32_t word(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        return bits;
    }

    SceneWriter& emit(EshiCommand op, const uint32_t* payload, uint32_t count) {
        if (overflowed_ || !words_ || size_ >= capacity_ ||
            count > capacity_ - size_ - 1) {
            overflowed_ = true;
            return *this;
        }
        words_[size_++] = ESHI_CMD_HEADER(op, count);
        for (uint32_t i = 0; i < count; ++i) words_[size_++] = payload[i];
        return *this;
    }

    uint32_t* words_;
    uint32_t  capacity_;
    uint32_t  size_;
    bool      overflowed_;
};

/** Reads back the packed event stream eshi_events_pack() produced. */
class EventReader {
  public:
    EventReader(const uint32_t* words, uint32_t count)
        : words_(words), count_(count), cursor_(0) {}

    /** Advances to the next collision, or returns false at the end. */
    bool next_collision(EshiCollisionEvent* out) {
        if (!words_ || !out) return false;
        while (cursor_ + 1 <= count_) {
            const uint32_t header = words_[cursor_];
            const uint32_t payload = ESHI_CMD_WORDS(header);
            if (payload > count_ - cursor_ - 1) return false;

            const uint32_t* p = words_ + cursor_ + 1;
            const uint32_t type = ESHI_CMD_OP(header);
            cursor_ += 1 + payload;

            if (type == ESHI_EVENT_COLLISION && payload >= 5) {
                out->a = (EshiEntity)p[0];
                out->b = (EshiEntity)p[1];
                out->nx = value(p[2]);
                out->ny = value(p[3]);
                out->penetration = value(p[4]);
                return true;
            }
        }
        return false;
    }

  private:
    static float value(uint32_t bits) {
        float f;
        std::memcpy(&f, &bits, sizeof f);
        return f;
    }

    const uint32_t* words_;
    uint32_t        count_;
    uint32_t        cursor_;
};

} /* namespace eshi */

#endif /* ESHI_SCENE_HPP */
