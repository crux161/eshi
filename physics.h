#pragma once
#include "glsl_core.h"
#include <cmath>

namespace Physics {
    
    struct AABB {
        glsl::vec2 pos;      
        glsl::vec2 halfSize; 
    };

    
    inline bool CheckCollision(const AABB& a, const AABB& b) {
        
        float deltaX = std::abs(a.pos.x - b.pos.x);
        float deltaY = std::abs(a.pos.y - b.pos.y);

        
        float intersectX = a.halfSize.x + b.halfSize.x;
        float intersectY = a.halfSize.y + b.halfSize.y;

        
        return (deltaX < intersectX) && (deltaY < intersectY);
    }

    
    
    inline bool ResolvePaddleBounce(const AABB& ball, const AABB& paddle, glsl::vec2& ballVel) {
        if (CheckCollision(ball, paddle)) {
            
            ballVel.x *= -1.0f; 
            
            
            float hitOffset = ball.pos.y - paddle.pos.y;
            ballVel.y += hitOffset * 0.05f; 
            
            
            ballVel.x *= 1.05f; 
            return true;
        }
        return false;
    }
}
