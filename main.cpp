#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glsl_core.h"
#include "renderer_cpu.h"
#include "display.h"
#include "input.h"
#include "game_interface.h"
#include "physics.h" 

#ifdef USE_OPENGL
    #include "renderer_gl.h"
#endif
#ifdef USE_CUDA
    #include "renderer_gpu.h"
#endif

#include <string>
#include <iostream>
#include <chrono>

using namespace glsl;

// --- GAME STATES ---
enum class GameState {
    MENU,
    PLAYING,
    GAME_OVER
};

// --- PONG GAME ---
class PongGame : public IGame {
    GameData data;
    const vec2 PADDLE_SIZE = vec2(0.05f, 0.2f);
    const vec2 BALL_SIZE = vec2(0.03f, 0.03f);
public:
    int scoreL = 0;
    int scoreR = 0;
    int winner = 0; // 0=None, 1=Left, 2=Right

    void Init() override {
        data = {};
        data.paddleL = vec2(-1.6f, 0.0f);
        data.paddleR = vec2( 1.6f, 0.0f);
        ResetBall();
        scoreL = 0;
        scoreR = 0;
        winner = 0;
    }
    
    void ResetBall() {
        data.ballPos = vec2(0.0f, 0.0f);
        data.ballVel = vec2(0.03f, 0.015f);
        data.hitTimer = 0.0f;
    }

    void Update(float dt) override {
        const float PADDLE_SPEED = 2.0f * dt;
        const float BOUND_Y = 1.0f;

        // Input
        if (Input::IsDown(SDL_SCANCODE_W)) data.paddleL.y += PADDLE_SPEED;
        if (Input::IsDown(SDL_SCANCODE_S)) data.paddleL.y -= PADDLE_SPEED;
        if (Input::IsDown(SDL_SCANCODE_UP))   data.paddleR.y += PADDLE_SPEED;
        if (Input::IsDown(SDL_SCANCODE_DOWN)) data.paddleR.y -= PADDLE_SPEED;

        // Constraints
        data.paddleL.y = clamp(data.paddleL.y, -BOUND_Y + PADDLE_SIZE.y, BOUND_Y - PADDLE_SIZE.y);
        data.paddleR.y = clamp(data.paddleR.y, -BOUND_Y + PADDLE_SIZE.y, BOUND_Y - PADDLE_SIZE.y);

        // Physics
        data.ballPos += data.ballVel;
        
        // Walls
        if (data.ballPos.y > BOUND_Y || data.ballPos.y < -BOUND_Y) data.ballVel.y *= -1.0f;

        // Paddles
        Physics::AABB ballBox   = { data.ballPos, BALL_SIZE };
        Physics::AABB paddleBoxL = { data.paddleL, PADDLE_SIZE };
        Physics::AABB paddleBoxR = { data.paddleR, PADDLE_SIZE };

        if (Physics::ResolvePaddleBounce(ballBox, paddleBoxL, data.ballVel)) data.hitTimer = 1.0f;
        if (Physics::ResolvePaddleBounce(ballBox, paddleBoxR, data.ballVel)) data.hitTimer = 1.0f;

        // Scoring
        if (data.ballPos.x > 2.0f) {
            scoreL++;
            ResetBall();
            data.ballVel.x = -0.03f; // Serve to winner
        }
        if (data.ballPos.x < -2.0f) {
            scoreR++;
            ResetBall();
            data.ballVel.x = 0.03f;
        }
        
        // Win Condition
        if (scoreL >= 5) winner = 1;
        if (scoreR >= 5) winner = 2;

        data.hitTimer *= 0.95f;
    }

    GameData* GetGameData() override { return &data; }
};

int main(int argc, char** argv) {
    int W = 960;
    int H = 540;
    bool use_gpu = true;

    for(int i=1; i<argc; i++){
        std::string arg = argv[i];
        if(arg == "--cpu") use_gpu = false;
    }

    Input::Init();
    Display window(W, H, "Eshi Game Engine - Pong");
    
    CpuRenderer* cpu_renderer = new CpuRenderer(W, H);
    GpuRenderer* gpu_renderer = nullptr;
    #ifdef USE_CUDA
    if (use_gpu) gpu_renderer = new GpuRenderer(W, H);
    #endif

    PongGame game;
    game.Init();
    
    GameState state = GameState::MENU;

    bool running = true;
    auto last_tick = std::chrono::high_resolution_clock::now();
    float time = 0.0f;

    while (running && window.isOpen()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last_tick).count();
        last_tick = now;
        time += dt;

        Input::Update();
        if (Input::IsPressed(SDL_SCANCODE_ESCAPE)) running = false;

        // --- STATE LOGIC ---
        if (state == GameState::MENU) {
            if (Input::IsPressed(SDL_SCANCODE_SPACE)) {
                game.Init(); // Reset game
                state = GameState::PLAYING;
            }
        } 
        else if (state == GameState::PLAYING) {
            game.Update(dt);
            if (game.winner != 0) state = GameState::GAME_OVER;
        }
        else if (state == GameState::GAME_OVER) {
            if (Input::IsPressed(SDL_SCANCODE_SPACE)) {
                state = GameState::MENU;
            }
        }

        // --- RENDER PIPELINE ---
        
        // 1. Render Simulation to Pixel Buffer
        int stride;
        uint8_t* pixels = window.get_pixel_buffer(stride);
        if (gpu_renderer) gpu_renderer->renderFrame(pixels, stride, time, game.GetGameData());
        else              cpu_renderer->renderFrame(pixels, stride, time, game.GetGameData());

        // 2. Composite Layers
        window.BeginFrame();
        
        // Draw the Game World (Layer 0)
        window.DrawGameLayer();

        // Draw UI (Layer 1)
        if (state == GameState::MENU) {
            // Darken background
            window.DrawUI_Box(0, 0, W, H, 0, 0, 0, 150); 
            window.DrawUI_Text("GLOW PONG", W/2 - 80, H/2 - 50, 0, 255, 255);
            window.DrawUI_Text("Press SPACE to Start", W/2 - 120, H/2 + 10);
        }
        else if (state == GameState::PLAYING) {
            // Scoreboard
            std::string s = std::to_string(game.scoreL) + "   " + std::to_string(game.scoreR);
            window.DrawUI_Text(s, W/2 - 30, 20, 200, 200, 200);
        }
        else if (state == GameState::GAME_OVER) {
            window.DrawUI_Box(0, 0, W, H, 50, 0, 0, 150);
            std::string winMsg = (game.winner == 1 ? "LEFT WINS!" : "RIGHT WINS!");
            window.DrawUI_Text(winMsg, W/2 - 80, H/2 - 20, 255, 255, 0);
            window.DrawUI_Text("Press SPACE to Return", W/2 - 120, H/2 + 40);
        }

        window.EndFrame();
    }

    delete cpu_renderer;
    if (gpu_renderer) delete gpu_renderer;
    // Window destructor handles SDL_Quit
    return 0;
}
