#pragma once
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
    return {lhs.x / scalar, lhs.y / scalar};
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