/**
 * @file pong.h
 * @brief First Light game surface: a uniform block, a scene, a shader.
 *
 * A host includes this, builds the scene into a world, and drives ticks. It
 * learns nothing about how Pong works, and Pong learns nothing about the host.
 */
#ifndef ESHI_EXAMPLE_PONG_H
#define ESHI_EXAMPLE_PONG_H

#include <eshi/eshi.h>
#include <eshi/shader.hpp>
#include <sumi/sumi.h>

namespace pong {

/**
 * The uniform block. Owned by the game, opaque to the engine.
 *
 * This is what used to be `struct GameData` inside glsl_core.h — a game's data
 * structure living in the engine's shared math header, which meant every new
 * game either edited that header or forked it. Nothing outside this file and
 * the shader knows these fields exist.
 */
struct Uniforms {
    sumi::vec2 paddle_l;
    sumi::vec2 paddle_r;
    sumi::vec2 ball;
    float      hit_timer;
    float      score_l;
    float      score_r;
};

/** Number of floats in the block, as the GPU tiers see it. */
const size_t kUniformFloats = 9;

/*
 * The GPU tiers receive this block as a flat float array and index it by
 * position (see the layout contract in pong.gpu.cpp). Padding or a reordered
 * member would silently garble the GPU render while leaving Ink correct, so the
 * layout is asserted rather than trusted.
 */
#if __cplusplus >= 201103L
static_assert(sizeof(Uniforms) == kUniformFloats * sizeof(float),
              "pong::Uniforms must stay tightly packed: pong.gpu.cpp indexes it as "
              "a flat float array");
static_assert(sizeof(sumi::vec2) == 2 * sizeof(float),
              "sumi::vec2 must be two tightly packed floats");
#endif

/** Handles into the world, plus the game's own state. */
struct Game {
    EshiEntity paddle_l;
    EshiEntity paddle_r;
    EshiEntity ball;
    EshiEntity wall[2];

    uint32_t epoch; /* scene submissions are monotonic; see reload() */
    int      score_l;
    int      score_r;
    Uniforms uniforms;
};

/**
 * Submits the scene description, registers systems, binds the material.
 *
 * Returns the material result: a host that ignores it will run a world with no
 * backend and present black frames.
 */
EshiResult build(EshiWorld* w, Game* g);

/**
 * Re-submits the scene description at a fresh epoch.
 *
 * The native stand-in for a Dart hot reload, and the thing §6.6 exists to make
 * safe: it runs the same description through the reconciler again mid-rally.
 * Nothing should move. `pong --hash --reload N` produces the same digest as
 * `pong --hash`, which is that claim measured at the framebuffer rather than
 * asserted in a comment.
 */
void reload(EshiWorld* w, Game* g);

/** Recentres the ball and sends it toward `direction` (-1 left, +1 right). */
void serve(EshiWorld* w, Game* g, float direction);

/** Fullscreen material entry point. */
void mainImage(sumi::vec4& fragColor, sumi::vec2 fragCoord,
               sumi::vec2 iResolution, float iTime, const Uniforms& u);

} /* namespace pong */

#endif /* ESHI_EXAMPLE_PONG_H */
