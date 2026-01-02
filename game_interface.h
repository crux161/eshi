#pragma once
#include "glsl_core.h" 

class IGame {
public:
    virtual ~IGame() {}
    virtual void Init() = 0;
    virtual void Update(float dt) = 0;
    
    
    virtual glsl::GameData* GetGameData() = 0; 
};
