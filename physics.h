#pragma once
#include "glsl_core.h"
#include <cmath>

namespace Physics {
    
    struct AABB {
        glsl::vec2 pos;      // Center position
        glsl::vec2 halfSize; // Half-width and Half-height (e.g., radius)
    };

    // Standard AABB vs AABB collision check
    inline bool CheckCollision(const AABB& a, const AABB& b) {
        // Calculate the distance between centers
        float deltaX = std::abs(a.pos.x - b.pos.x);
        float deltaY = std::abs(a.pos.y - b.pos.y);

        // Calculate the combined size (minimum safe distance)
        float intersectX = a.halfSize.x + b.halfSize.x;
        float intersectY = a.halfSize.y + b.halfSize.y;

        // If distance is less than combined size on BOTH axes, we are colliding
        return (deltaX < intersectX) && (deltaY < intersectY);
    }

    // Helper to resolve collision (Basic reflection for Pong)
    // Returns true if velocity should flip
    inline bool ResolvePaddleBounce(const AABB& ball, const AABB& paddle, glsl::vec2& ballVel) {
        if (CheckCollision(ball, paddle)) {
            // Simple bounce: Flip X velocity
            ballVel.x *= -1.0f; 
            
            // Optional: Add a little "english" (spin) based on where it hit the paddle
            float hitOffset = ball.pos.y - paddle.pos.y;
            ballVel.y += hitOffset * 0.05f; 
            
            // Increase speed slightly for fun
            ballVel.x *= 1.05f; 
            return true;
        }
        return false;
    }
}
