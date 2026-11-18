#pragma once
#include "entity.hpp"

class Player final : public Entity {
public:
    Player(Vector2 pos, float radius) : pos_(pos), radius_(radius) {}

    void update(float dt) override { (void)dt; }
    float radius() const override { return radius_; }
    Vector2 position() const override { return pos_; }

    void grow(float amount) { radius_ += amount; }
    void set_position(Vector2 pos) { pos_ = pos; }

private:
    Vector2 pos_;
    float radius_;
};
