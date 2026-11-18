#pragma once
#include "vector2d.hpp"

// Пара 3. Контракт сущности. Удаление через Entity* обязано вызвать деструктор наследника.
class Entity {
public:
    virtual ~Entity() = default;

    virtual void update(float dt) = 0;
    virtual float radius() const = 0;
    virtual Vector2 position() const = 0;

    // Пересечение кругов. Не меняет объекты.
    bool intersects(const Entity& other) const {
        const Vector2 delta = position() + Vector2{-other.position().x(), -other.position().y()};
        const float reach = radius() + other.radius();
        return delta.length() < reach;
    }
};
