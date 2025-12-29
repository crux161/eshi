#pragma once
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

class Display {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Texture* texture = NULL;
    int width, height;
    bool open = true;
    uint32_t last_tick;

public:
    Display(int w, int h, const char* title) : width(w), height(h) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
            exit(1);
        }

        window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                  width, height, SDL_WINDOW_SHOWN);
        if (!window) { fprintf(stderr, "Window Error: %s\n", SDL_GetError()); exit(1); }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
        
        last_tick = SDL_GetTicks();
    }

    ~Display() {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool isOpen() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) open = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) open = false;
        }
        return open;
    }

    
    uint8_t* get_pixel_buffer(int& out_pitch) {
        void* pixels;
        SDL_LockTexture(texture, NULL, &pixels, &out_pitch);
        return (uint8_t*)pixels;
    }

    void submit_frame() {
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        
        
        
        
        
    }
};
