#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <iostream>
#include <vector>

class Display {
    int width, height;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    std::vector<uint8_t> backBuffer;

public:
    Display(int w, int h, const std::string& title) : width(w), height(h) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) exit(1);
        window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                  width, height, SDL_WINDOW_SHOWN);
        
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, 
                                    SDL_TEXTUREACCESS_STREAMING, width, height);
        backBuffer.resize(width * height * 4);
    }

    ~Display() {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool isOpen() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) return false;
        }
        return true;
    }

    uint8_t* get_pixel_buffer(int& stride) {
        stride = width * 4;
        return backBuffer.data();
    }

    void submit_frame() {
        SDL_UpdateTexture(texture, NULL, backBuffer.data(), width * 4);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
};
