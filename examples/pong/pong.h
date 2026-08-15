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

/** Handles into the world, plus the game's own state. */
struct Game {
    EshiEntity paddle_l;
    EshiEntity paddle_r;
    EshiEntity ball;
    EshiEntity wall[2];

    int      score_l;
    int      score_r;
    Uniforms uniforms;
};

/** Creates entities, registers systems, binds the material. */
void build(EshiWorld* w, Game* g);

/** Recentres the ball and sends it toward `direction` (-1 left, +1 right). */
void serve(EshiWorld* w, Game* g, float direction);

/** Fullscreen material entry point. */
void mainImage(sumi::vec4& fragColor, sumi::vec2 fragCoord,
               sumi::vec2 iResolution, float iTime, const Uniforms& u);

} /* namespace pong */

#endif /* ESHI_EXAMPLE_PONG_H */
