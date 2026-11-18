#pragma once

#include "entity.hpp"

// Тема: второй наследник той же базы.
//
// Еда реализует тот же контракт, что и игрок, но update пустой: еда не
// едет по вводу. Мир вызывает update через Entity*, не зная этот факт.
// Полиморфизм здесь — выбор функции по реальному объекту в куче, а не
// по типу указателя, который в цикле мира всегда Entity*.
//
// Радиус константой, не полем, потому что еда в этой модели не растёт.
// Позиция всё равно поле: её задают при спавне.
class Food final : public Entity {
public:
    explicit Food(Vector2 pos) : pos_(pos) {}

    void update(float) override {}

    float radius() const override { return 5.f; }
    Vector2 position() const override { return pos_; }

private:
    Vector2 pos_;
};
