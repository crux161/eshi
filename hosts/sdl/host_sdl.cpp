/**
 * @file host_sdl.cpp
 * @brief Standalone desktop host — the only file in this path that knows SDL.
 *
 * Responsibilities, and nothing else: create a surface, translate native input
 * into abstract EshiKeys, drive the frame loop, and present or encode the
 * framebuffer. The core never learns any of it.
 *
 * The loop is host-owned by design. Under the Flutter embedder (Phase 3) the
 * vsync callback drives the same eshi_tick/eshi_render pair, which is why
 * neither of them may ever block or own a loop of its own.
 */
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <eshi/eshi.h>

#include "../../examples/pong/pong.h"
#include "../../encoder.h"

namespace {

struct KeyMap {
    SDL_Scancode scancode;
    EshiKey      key;
};

const KeyMap kKeyMap[] = {
    { SDL_SCANCODE_UP,     ESHI_KEY_UP },
    { SDL_SCANCODE_DOWN,   ESHI_KEY_DOWN },
    { SDL_SCANCODE_LEFT,   ESHI_KEY_LEFT },
    { SDL_SCANCODE_RIGHT,  ESHI_KEY_RIGHT },
    { SDL_SCANCODE_W,      ESHI_KEY_W },
    { SDL_SCANCODE_A,      ESHI_KEY_A },
    { SDL_SCANCODE_S,      ESHI_KEY_S },
    { SDL_SCANCODE_D,      ESHI_KEY_D },
    { SDL_SCANCODE_SPACE,  ESHI_KEY_SPACE },
    { SDL_SCANCODE_ESCAPE, ESHI_KEY_ESCAPE },
};

const int kKeyMapCount = (int)(sizeof(kKeyMap) / sizeof(kKeyMap[0]));

/** Pushes the current keyboard state across the boundary as abstract keys. */
void pump_input(EshiWorld* w) {
    const Uint8* state = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < kKeyMapCount; ++i) {
        eshi_input_set_key(w, kKeyMap[i].key, state[kKeyMap[i].scancode] ? 1 : 0);
    }
}

/**
 * FNV-1a over the raw framebuffer.
 *
 * The Ink tier is the reference oracle the GPU tiers get diffed against, so a
 * cheap content hash of the framebuffer — taken before any encoder touches it —
 * is engine infrastructure rather than a debugging aid. It is also the only way
 * to test determinism without a video container's metadata in the way.
 */
uint64_t hash_framebuffer(const uint8_t* pixels, int width, int height, int stride) {
    uint64_t h = 1469598103934665603ull;
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = pixels + (ptrdiff_t)y * stride;
        for (int i = 0; i < width * 4; ++i) {
            h ^= row[i];
            h *= 1099511628211ull;
        }
    }
    return h;
}

void print_usage(const char* argv0) {
    std::printf(
        "usage: %s [--live] [--res WxH] [--frames N] [--seed N] [--out FILE] [--hash]\n"
        "\n"
        "  --live       Open a window instead of encoding to a file.\n"
        "  --res WxH    Framebuffer resolution. Default 960x540.\n"
        "  --frames N   Frames to encode in headless mode. Default 600.\n"
        "  --seed N     RNG seed. Equal seeds produce byte-identical output.\n"
        "  --out FILE   Output path. Default pong.mp4.\n"
        "  --hash       Render without encoding and print a framebuffer digest.\n",
        argv0);
}

} /* namespace */

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int         width = 960;
    int         height = 540;
    bool        live = false;
    bool        hash_only = false;
    int         frames = 600;
    uint64_t    seed = 0x5EED5EEDull;
    std::string out_path = "pong.mp4";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--live") {
            live = true;
        } else if (arg == "--res" && i + 1 < argc) {
            const std::string res = argv[++i];
            const size_t x = res.find('x');
            if (x != std::string::npos) {
                width = std::atoi(res.substr(0, x).c_str());
                height = std::atoi(res.substr(x + 1).c_str());
            }
        } else if (arg == "--frames" && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = (uint64_t)std::strtoull(argv[++i], NULL, 10);
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--hash") {
            hash_only = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (width <= 0 || height <= 0 || frames <= 0) {
        std::fprintf(stderr, "invalid resolution or frame count\n");
        return 1;
    }

    EshiConfig cfg = eshi_config_default();
    cfg.width = width;
    cfg.height = height;
    cfg.grade = ESHI_GRADE_INK;
    cfg.seed = seed;

    EshiWorld* world = eshi_world_create(&cfg);
    if (!world) {
        std::fprintf(stderr, "eshi_world_create failed\n");
        return 1;
    }

    std::printf("Eshi/Larimar  grade=%s  %dx%d  seed=%llu\n",
                eshi_grade_string(eshi_world_grade(world)),
                width, height, (unsigned long long)seed);

    pong::Game game;
    std::memset(&game, 0, sizeof(game));
    pong::build(world, &game);

    int rc = 0;

    if (live) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            eshi_world_destroy(world);
            return 1;
        }

        SDL_Window* window = SDL_CreateWindow(
            "Eshi — Larimar First Light",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        SDL_Renderer* renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        SDL_Texture* texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);

        std::vector<uint8_t> framebuffer((size_t)width * (size_t)height * 4);
        const int stride = width * 4;

        Uint64 previous = SDL_GetPerformanceCounter();
        const double freq = (double)SDL_GetPerformanceFrequency();
        bool running = true;

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
            }

            const Uint64 now = SDL_GetPerformanceCounter();
            float dt = (float)((double)(now - previous) / freq);
            previous = now;
            if (dt > 0.25f) dt = 0.25f;

            pump_input(world);
            eshi_tick(world, dt);
            if (eshi_input_down(world, ESHI_KEY_ESCAPE)) running = false;

            eshi_render(world, framebuffer.data(), stride, (float)eshi_sim_time(world));

            SDL_UpdateTexture(texture, NULL, framebuffer.data(), stride);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

    } else if (hash_only) {
        /* Same loop as the encode path, with the encoder removed. */
        std::vector<uint8_t> framebuffer((size_t)width * (size_t)height * 4);
        const int stride = width * 4;
        const float fixed_dt = 1.0f / 60.0f;
        uint64_t digest = 1469598103934665603ull;

        for (int i = 0; i < frames; ++i) {
            eshi_tick(world, fixed_dt);
            eshi_render(world, framebuffer.data(), stride, (float)eshi_sim_time(world));
            digest ^= hash_framebuffer(framebuffer.data(), width, height, stride);
            digest *= 1099511628211ull;
        }
        std::printf("frames=%d digest=%016llx score=%d-%d\n",
                    frames, (unsigned long long)digest, game.score_l, game.score_r);

    } else {
        /*
         * Headless encode. dt is exactly the fixed step and time comes from the
         * simulation clock, never the wall clock — the two things (with the
         * seeded RNG) that make the same command produce the same bytes.
         */
        std::printf("Encoding %d frames -> %s\n", frames, out_path.c_str());

        SimpleEncoder video(out_path.c_str(), width, height, 60);
        const float fixed_dt = 1.0f / 60.0f;

        for (int i = 0; i < frames; ++i) {
            int stride = 0;
            uint8_t* pixels = video.get_pixel_buffer(stride);

            eshi_tick(world, fixed_dt);
            eshi_render(world, pixels, stride, (float)eshi_sim_time(world));

            video.submit_frame();
        }
        std::printf("\nDone. Final score %d - %d\n", game.score_l, game.score_r);
    }

    eshi_world_destroy(world);
    return rc;
}
