#pragma once
#include "glsl_core.h" // For types like vec2, GameData

class IGame {
public:
    virtual ~IGame() {}
    virtual void Init() = 0;
    virtual void Update(float dt) = 0;
    
    // Returns the data needed by the GPU/CPU renderer
    virtual glsl::GameData* GetGameData() = 0; 
};
