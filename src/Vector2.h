#pragma once

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

inline Vector2 operator*(const Vector2& lhs, float scalar) {
    return {lhs.x * scalar, lhs.y * scalar};
}

inline Vector2 operator/(const Vector2& lhs, float scalar) {
    return {lhs.x / scalar, lhs.y / scalar};
}
