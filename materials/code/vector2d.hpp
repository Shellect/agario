#pragma once
#include <cmath>

// Пара 2. Тип-значение: его можно копировать, у него нет сокета и буфера.
struct Vector2 {
    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) noexcept : x_(x), y_(y) {}

    constexpr float x() const noexcept { return x_; }
    constexpr float y() const noexcept { return y_; }

    constexpr Vector2 operator+(Vector2 other) const noexcept {
        return {x_ + other.x_, y_ + other.y_};
    }

    float length() const noexcept {
        return std::sqrt(x_ * x_ + y_ * y_);
    }

    friend constexpr bool operator==(Vector2 a, Vector2 b) noexcept {
        return a.x_ == b.x_ && a.y_ == b.y_;
    }

private:
    float x_ = 0.f;
    float y_ = 0.f;
};
