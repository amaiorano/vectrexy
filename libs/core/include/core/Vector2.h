#pragma once
#include <algorithm>
#include <cmath>

struct Vector2 {
    float x = 0.f;
    float y = 0.f;

    void operator+=(const Vector2& rhs) {
        x += rhs.x;
        y += rhs.y;
    }
};

inline Vector2 operator+(const Vector2& lhs, const Vector2& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Vector2 operator-(const Vector2& lhs, const Vector2& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Vector2 operator*(const Vector2& lhs, float scalar) {
    return {lhs.x * scalar, lhs.y * scalar};
}

inline Vector2 operator/(const Vector2& lhs, float scalar) {
    if (scalar == 0.f) {
        return {0.f, 0.f};
    }
    return {lhs.x / scalar, lhs.y / scalar};
}

inline Vector2 operator/(const Vector2& lhs, const Vector2& rhs) {
    return {rhs.x == 0.f ? 0.f : lhs.x / rhs.x, rhs.y == 0.f ? 0.f : lhs.y / rhs.y};
}

inline bool operator==(const Vector2& lhs, const Vector2& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline float Magnitude(const Vector2& v) {
    return ::sqrt(v.x * v.x + v.y * v.y);
}

inline Vector2 Normalized(const Vector2& v) {
    auto mag = Magnitude(v);
    return mag > 0 ? v / Magnitude(v) : Vector2{0.f, 0.f};
}

inline Vector2 Abs(const Vector2& v) {
    return {std::abs(v.x), std::abs(v.y)};
}

inline Vector2 Clamp(const Vector2& v, float min, float max) {
    return {std::clamp(v.x, min, max), std::clamp(v.y, min, max)};
}
