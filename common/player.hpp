#pragma once

#include "entity.hpp"

#include <cstdint>

// Тема: публичное наследование, override и final.
//
// Публичное наследование читается как «игрок является сущностью мира»,
// поэтому Player можно хранить как unique_ptr<Entity>. final закрывает
// иерархию снизу: бот и человек отличаются тем, откуда приходит направление
// (сеть или функция), а не отдельным подклассом. Раннее дерево
// «игрок → бот → особенный бот» заставляет мир плодить dynamic_cast.
//
// override просит компилятор найти в базе метод с той же сигнатурой.
// Опечатка (double вместо float, забытый const) без override создаёт новую
// функцию, класс остаётся абстрактным, а диагностика указывает в сторону.
// С override ошибка стоит на самой строке.
//
// Игрок — почти значение (числа и идентификатор), но живёт он в куче под
// уникальным указателем мира, потому что лежит в одном контейнере с едой.
// Копирование самого Player не запрещено: в нём нет сокета. Копировать его
// в обход мира всё равно не нужно — хозяином клетки является World.
class Player final : public Entity {
public:
    Player(std::uint32_t id, Vector2 pos, float radius)
        : id_(id), pos_(pos), radius_(radius) {}

    void update(float dt) override {
        // Скорость падает с ростом: крупную клетку труднее разогнать.
        // normalized() гасит длинный вектор мыши, чтобы диагональ не была быстрее осей.
        const float speed = 160.f / (1.f + radius_ * 0.02f);
        pos_ = pos_ + dir_.normalized() * (speed * dt);
    }

    float radius() const override { return radius_; }
    Vector2 position() const override { return pos_; }

    std::uint32_t id() const noexcept { return id_; }

    void set_direction(Vector2 dir) { dir_ = dir; }

    // Позицию пишет только мир (границы карты). Сессия направления меняет
    // set_direction и не телепортирует клетку: сервер остаётся хозяином координат.
    void set_position(Vector2 pos) { pos_ = pos; }

    void grow(float amount) { radius_ += amount; }

private:
    std::uint32_t id_ = 0;
    Vector2 pos_{};
    Vector2 dir_{};
    float radius_ = 16.f;
};
