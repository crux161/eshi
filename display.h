#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <iostream>
#include <vector>

class Display {
    int width, height;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* gameTexture; // The pixel buffer from your engine
    std::vector<uint8_t> backBuffer; // CPU side buffer

    // Font support
    TTF_Font* font;

public:
    Display(int w, int h, const std::string& title) : width(w), height(h), font(nullptr) {
        // Init Video and TTF
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
            exit(1);
        }
        if (TTF_Init() == -1) {
            std::cerr << "TTF Error: " << TTF_GetError() << std::endl;
            exit(1);
        }

        // Create Window & Renderer
        window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                  width, height, SDL_WINDOW_SHOWN);
        
        // Use Hardware Acceleration
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        // Create the texture for the game pixels (Streaming allows frequent updates)
        gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, 
                                        SDL_TEXTUREACCESS_STREAMING, width, height);

        backBuffer.resize(width * height * 4);
        
        // Load a default font (You might need to adjust this path!)
        // On Linux, standard fonts are usually here:
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24);
        if (!font) {
            std::cerr << "Warning: Failed to load font. Text will not render.\n";
            std::cerr << "TTF Error: " << TTF_GetError() << std::endl;
        }
    }

    ~Display() {
        if (font) TTF_CloseFont(font);
        SDL_DestroyTexture(gameTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        // We don't SDL_Quit here because main handles it
    }

    bool isOpen() { return true; } // Main loop handles events now

    // Get the buffer we write our pixels to
    uint8_t* get_pixel_buffer(int& stride) {
        stride = width * 4;
        return backBuffer.data();
    }

    // --- RENDERING PIPELINE ---

    void BeginFrame() {
        // Clear screen to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    void DrawGameLayer() {
        // Update the texture with our CPU/GPU pixel buffer
        SDL_UpdateTexture(gameTexture, NULL, backBuffer.data(), width * 4);
        // Copy to entire screen
        SDL_RenderCopy(renderer, gameTexture, NULL, NULL);
    }

    void DrawUI_Text(const std::string& text, int x, int y, int r=255, int g=255, int b=255) {
        if (!font) return;

        SDL_Color color = { (Uint8)r, (Uint8)g, (Uint8)b, 255 };
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            
            SDL_Rect dstRect = { x, y, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, NULL, &dstRect);

            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }
    }
    
    // Draw a semi-transparent box (good for menu backgrounds)
    void DrawUI_Box(int x, int y, int w, int h, int r, int g, int b, int a) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        SDL_Rect rect = { x, y, w, h };
        SDL_RenderFillRect(renderer, &rect);
    }

    void EndFrame() {
        SDL_RenderPresent(renderer);
    }
};
