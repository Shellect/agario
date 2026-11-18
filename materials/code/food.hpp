#pragma once
#include "entity.hpp"

class Food final : public Entity {
public:
    explicit Food(Vector2 pos) : pos_(pos) {}

    void update(float) override {}
    float radius() const override { return 4.f; }
    Vector2 position() const override { return pos_; }

private:
    Vector2 pos_;
};
