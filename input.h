#pragma once
#include <SDL2/SDL.h>
#include <cstring>

class Input {
public:
    static void Init() {
        
        keyboardState = SDL_GetKeyboardState(nullptr);
        memset(prevKeyboardState, 0, sizeof(prevKeyboardState));
    }

    static void Update() {
        
        
        memcpy(prevKeyboardState, keyboardState, SDL_NUM_SCANCODES);
        
        
        SDL_PumpEvents(); 
        
        
    }

    
    static bool IsDown(SDL_Scancode key) {
        return keyboardState[key];
    }

    
    static bool IsPressed(SDL_Scancode key) {
        return keyboardState[key] && !prevKeyboardState[key];
    }

    
    static bool IsReleased(SDL_Scancode key) {
        return !keyboardState[key] && prevKeyboardState[key];
    }

private:
    static const Uint8* keyboardState;
    static Uint8 prevKeyboardState[SDL_NUM_SCANCODES];
};


const Uint8* Input::keyboardState = nullptr;
Uint8 Input::prevKeyboardState[SDL_NUM_SCANCODES];
