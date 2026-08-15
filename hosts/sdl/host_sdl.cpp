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
#include <eshi/shader.hpp>

#include "../../examples/pong/pong.h"
#include "../../encoder.h"

/*
 * A gallery shader, linked in for the Ink tier and handed to the GPU tiers as a
 * source path. One file, both roles — which is what makes cross-tier pixel
 * comparison meaningful: Ink is the reference oracle, and the GPU tiers are
 * checked against it. Defined by examples/ripple.cpp at global scope.
 */
void mainImage(sumi::vec4& fragColor, sumi::vec2 fragCoord,
               sumi::vec2 iResolution, float iTime);

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

EshiGrade parse_grade(const std::string& name, bool* ok) {
    *ok = true;
    if (name == "ink")   return ESHI_GRADE_INK;
    if (name == "paper") return ESHI_GRADE_PAPER;
    if (name == "brush") return ESHI_GRADE_BRUSH;
    if (name == "gold")  return ESHI_GRADE_GOLD;
    *ok = false;
    return ESHI_GRADE_INK;
}

/** Trampoline so the core can resolve GL symbols without linking SDL. */
void* gl_proc_loader(const char* name) {
    return (void*)SDL_GL_GetProcAddress(name);
}

void print_usage(const char* argv0) {
    std::printf(
        "usage: %s [--live] [--res WxH] [--frames N] [--seed N] [--out FILE]\n"
        "       %*s [--hash] [--grade G] [--gallery]\n"
        "\n"
        "  --live       Open a window instead of encoding to a file.\n"
        "  --res WxH    Framebuffer resolution. Default 960x540.\n"
        "  --frames N   Frames to encode in headless mode. Default 600.\n"
        "  --seed N     RNG seed. Equal seeds produce byte-identical output.\n"
        "  --out FILE   Output path. Default pong.mp4.\n"
        "  --hash       Render without encoding and print a framebuffer digest.\n"
        "  --grade G    Kantei tier: ink, paper, brush, gold. Default ink.\n"
        "  --gallery    Run the ripple gallery shader instead of pong. Required\n"
        "               for the GPU tiers, which cannot execute pong's typed\n"
        "               uniform struct.\n",
        argv0, (int)std::strlen(argv0), "");
}

} /* namespace */

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int         width = 960;
    int         height = 540;
    bool        live = false;
    bool        hash_only = false;
    bool        gallery = false;
    EshiGrade   grade = ESHI_GRADE_INK;
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
        } else if (arg == "--gallery") {
            gallery = true;
        } else if (arg == "--grade" && i + 1 < argc) {
            bool ok = false;
            grade = parse_grade(argv[++i], &ok);
            if (!ok) {
                std::fprintf(stderr, "unknown grade: %s\n", argv[i]);
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (width <= 0 || height <= 0 || frames <= 0) {
        std::fprintf(stderr, "invalid resolution or frame count\n");
        return 1;
    }

    /*
     * The Paper tier needs a GL context, and the core cannot create one — it
     * links no windowing library. So the host makes one (hidden, since we
     * render offscreen and read back) and hands over its symbol loader.
     */
    SDL_Window*   gl_window = NULL;
    SDL_GLContext gl_context = NULL;
    if (grade == ESHI_GRADE_PAPER) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        gl_window = SDL_CreateWindow("eshi-gl", 0, 0, 16, 16,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        gl_context = gl_window ? SDL_GL_CreateContext(gl_window) : NULL;
        if (!gl_context) {
            std::fprintf(stderr, "failed to create a GL 3.3 context: %s\n", SDL_GetError());
            return 1;
        }
        eshi_gl_set_proc_loader(gl_proc_loader);
    }

    if (!eshi_grade_available(grade)) {
        std::fprintf(stderr,
                     "grade '%s' is not available in this build (best available: %s)\n",
                     eshi_grade_string(grade), eshi_grade_string(eshi_grade_best()));
        return 1;
    }

    EshiConfig cfg = eshi_config_default();
    cfg.width = width;
    cfg.height = height;
    cfg.grade = grade;
    cfg.seed = seed;

    EshiWorld* world = eshi_world_create(&cfg);
    if (!world) {
        std::fprintf(stderr, "eshi_world_create failed\n");
        return 1;
    }

    std::printf("Eshi/Larimar  grade=%s  %dx%d  seed=%llu  scene=%s\n",
                eshi_grade_string(eshi_world_grade(world)),
                width, height, (unsigned long long)seed,
                gallery ? "ripple" : "pong");

    pong::Game game;
    std::memset(&game, 0, sizeof(game));

    if (gallery) {
        /*
         * The same source file in both roles: compiled in for Ink, handed to
         * the GPU tiers as a path they transpile at runtime. Comparing the two
         * digests is the cross-tier conformance check.
         */
        EshiMaterial material;
        material.cpu_shader = eshi::shader_no_uniforms<mainImage>();
        material.source_path = "examples/ripple.cpp";
        material.uniform_data = NULL;
        material.uniform_size = 0;

        const EshiResult rc_material = eshi_material_set(world, &material);
        if (rc_material != ESHI_OK) {
            std::fprintf(stderr, "eshi_material_set failed: %s\n",
                         eshi_result_string(rc_material));
            eshi_world_destroy(world);
            return 1;
        }
    } else {
        if (grade != ESHI_GRADE_INK) {
            std::fprintf(stderr,
                         "pong runs on Ink only: its shader takes a typed uniform "
                         "struct the transpiler cannot lower. Use --gallery.\n");
            eshi_world_destroy(world);
            return 1;
        }
        pong::build(world, &game);
    }

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

    if (gl_context) SDL_GL_DeleteContext(gl_context);
    if (gl_window) SDL_DestroyWindow(gl_window);

    return rc;
}
