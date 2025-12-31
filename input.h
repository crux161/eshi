#pragma once
#include <SDL2/SDL.h>
#include <cstring>

class Input {
public:
    static void Init() {
        // SDL internal array pointer, valid for entire lifetime
        keyboardState = SDL_GetKeyboardState(nullptr);
        memset(prevKeyboardState, 0, sizeof(prevKeyboardState));
    }

    static void Update() {
        // Copy current state to previous state for "Just Pressed" detection
        // Note: SDL_GetKeyboardState returns a live pointer, so we copy the *values* for history
        memcpy(prevKeyboardState, keyboardState, SDL_NUM_SCANCODES);
        
        // Pump events to update the live pointer
        SDL_PumpEvents(); 
        
        // Mouse handling can go here later
    }

    // Is the key currently held down?
    static bool IsDown(SDL_Scancode key) {
        return keyboardState[key];
    }

    // Was the key just pressed this frame?
    static bool IsPressed(SDL_Scancode key) {
        return keyboardState[key] && !prevKeyboardState[key];
    }

    // Was the key just released?
    static bool IsReleased(SDL_Scancode key) {
        return !keyboardState[key] && prevKeyboardState[key];
    }

private:
    static const Uint8* keyboardState;
    static Uint8 prevKeyboardState[SDL_NUM_SCANCODES];
};

// Static definition
const Uint8* Input::keyboardState = nullptr;
Uint8 Input::prevKeyboardState[SDL_NUM_SCANCODES];
