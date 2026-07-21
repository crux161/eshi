#include "../../glsl_core.h"

using namespace glsl;

struct MapResult {
    float distance;
    float material;
};

SHADER_CTX float harlequinHash(vec2 p) {
    return glsl::fract(glsl::sin(glsl::dot(p, vec2(12.9898f, 78.233f))) * 43758.5453f);
}

SHADER_CTX MapResult harlequinMap(vec3 p, float iTime) {
    float bounceHeight = glsl::abs(glsl::sin(iTime * 3.0f)) * 2.0f;
    float sphere = glsl::length(p - vec3(0.0f, 1.0f + bounceHeight, 0.0f)) - 1.0f;
    float floorDistance = p.y;

    MapResult result;
    if (sphere < floorDistance) {
        result.distance = sphere;
        result.material = 1.0f;
    } else {
        result.distance = floorDistance;
        result.material = 0.0f;
    }
    return result;
}

SHADER_CTX vec3 harlequinNormal(vec3 p, float iTime) {
    const float e = 0.001f;
    float x = harlequinMap(p + vec3(e, 0.0f, 0.0f), iTime).distance
            - harlequinMap(p - vec3(e, 0.0f, 0.0f), iTime).distance;
    float y = harlequinMap(p + vec3(0.0f, e, 0.0f), iTime).distance
            - harlequinMap(p - vec3(0.0f, e, 0.0f), iTime).distance;
    float z = harlequinMap(p + vec3(0.0f, 0.0f, e), iTime).distance
            - harlequinMap(p - vec3(0.0f, 0.0f, e), iTime).distance;
    return glsl::normalize(vec3(x, y, z));
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    float cameraAngle = iTime * 0.4f;
    float radius = 8.0f;
    vec3 rayOrigin = vec3(radius * glsl::sin(cameraAngle), 2.5f,
                          -radius * glsl::cos(cameraAngle));

    vec3 target = vec3(0.0f, 1.0f, 0.0f);
    vec3 cameraForward = glsl::normalize(target - rayOrigin);
    vec3 cameraRight = glsl::normalize(glsl::cross(cameraForward, vec3(0.0f, 1.0f, 0.0f)));
    vec3 cameraUp = glsl::normalize(glsl::cross(cameraRight, cameraForward));
    vec3 rayDirection = glsl::normalize(cameraRight * uv.x + cameraUp * uv.y + cameraForward);

    float distance = 0.0f;
    float material = 0.0f;
    vec3 position = rayOrigin;
    for (int i = 0; i < 100; ++i) {
        position = rayOrigin + rayDirection * distance;
        MapResult result = harlequinMap(position, iTime);
        material = result.material;
        if (result.distance < 0.001f || distance > 40.0f) break;
        distance += result.distance;
    }

    vec3 color = vec3(0.0f);
    const vec3 mistColor = vec3(0.05f, 0.08f, 0.18f);
    if (distance < 40.0f) {
        vec3 normal = harlequinNormal(position, iTime);
        vec3 light = glsl::normalize(vec3(1.0f, 2.0f, -1.0f));
        float diffuse = glsl::max(0.0f, glsl::dot(normal, light));

        if (material < 0.5f) {
            float checkerCoordinate = glsl::floor(position.x) + glsl::floor(position.z);
            float checker = checkerCoordinate - 2.0f * glsl::floor(checkerCoordinate * 0.5f);
            vec3 baseColor = checker < 0.5f
                ? vec3(0.08f, 0.08f, 0.12f)
                : vec3(0.85f, 0.85f, 0.90f);

            vec2 sparkleCell = glsl::floor(position.xz() * 12.0f);
            float noise = harlequinHash(sparkleCell);
            float twinkle = glsl::pow(
                glsl::max(0.0f, glsl::sin(iTime * 6.0f + noise * 62.83f) * 0.5f + 0.5f),
                12.0f);
            vec3 sparkle = vec3(1.0f, 0.9f, 0.5f) * twinkle * 1.5f;
            if (checker >= 0.5f) sparkle = vec3(0.0f);
            color = baseColor * (diffuse + 0.1f) + sparkle;
        } else {
            color = vec3(0.9f, 0.2f, 0.35f) * (diffuse + 0.15f);
        }
    }

    float fog = 1.0f - glsl::exp(-distance * 0.05f);
    color = glsl::mix(color, mistColor, fog);
    color = glsl::pow(glsl::max(color, vec3(0.0f)), vec3(1.0f / 2.2f));
    fragColor = vec4(color, 1.0f);
}
