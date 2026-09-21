#pragma once

#include <algorithm>
#include <cmath>

namespace ShaderMath {

struct Vec2 {
    float x;
    float y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

inline Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 operator*(Vec2 v, float s) {
    return {v.x * s, v.y * s};
}

inline Vec2 operator*(float s, Vec2 v) {
    return v * s;
}

inline Vec2 operator/(Vec2 v, float s) {
    return {v.x / s, v.y / s};
}

inline Vec2& operator+=(Vec2& a, Vec2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

inline Vec2& operator-=(Vec2& a, Vec2 b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

inline float length(Vec2 v) {
    return std::hypot(v.x, v.y);
}

inline Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator*(Vec3 v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline Vec3 operator*(float s, Vec3 v) {
    return v * s;
}

inline Vec3 operator+(Vec3 v, float s) {
    return {v.x + s, v.y + s, v.z + s};
}

inline Vec3 operator*(Vec3 a, Vec3 b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

}  // namespace ShaderMath
