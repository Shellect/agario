#pragma once

#include <cmath>

// Тема: тип-значение.
//
// Vector2 полностью описан двумя числами. Два вектора с одинаковыми
// координатами взаимозаменяемы: у значения нет «идентичности» вроде
// дескриптора сокета. Поэтому компиляторные копирование и перемещение
// здесь правильные, их не запрещают и не пишут вручную.
//
// Это противоположность TcpSession. Сессия владеет соединением операционной
// системы, почленная копия дескриптора создала бы двух хозяев одного сокета.
// Вектор координат так не устроен, его передают и по значению, и по
// константной ссылке — обе формы честные.
//
// const у методов — контракт «наблюдаемое состояние не меняется». Через
// const Vector2& можно вызвать x(), y(), length() и operator+. Присваивание
// в поле из такого метода не компилируется: this внутри const-метода имеет
// тип const Vector2*.
//
// noexcept на этих операциях — обещание не бросать исключение. Сложение двух
// float само по себе исключения не бросает. vector при переносе элементов
// опирается на небросающее перемещение; у значения из двух чисел перемещение
// совпадает с копированием, но пометка уже учит отделять честный noexcept
// от функции, которая открывает файл или парсит сеть.
struct Vector2 {
    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) noexcept : x_(x), y_(y) {}

    constexpr float x() const noexcept { return x_; }
    constexpr float y() const noexcept { return y_; }

    constexpr Vector2 operator+(Vector2 other) const noexcept {
        return {x_ + other.x_, y_ + other.y_};
    }

    constexpr Vector2 operator-(Vector2 other) const noexcept {
        return {x_ - other.x_, y_ - other.y_};
    }

    constexpr Vector2 operator*(float scale) const noexcept {
        return {x_ * scale, y_ * scale};
    }

    float length() const noexcept {
        return std::sqrt(x_ * x_ + y_ * y_);
    }

    // Нулевое направление (игрок отпустил ввод) остаётся нулём.
    // Деление на длину без этой проверки дало бы inf/nan и разъехавшуюся симуляцию.
    Vector2 normalized() const noexcept {
        const float len = length();
        if (len < 1e-4f) {
            return {};
        }
        return (*this) * (1.f / len);
    }

    friend constexpr bool operator==(Vector2 a, Vector2 b) noexcept {
        return a.x_ == b.x_ && a.y_ == b.y_;
    }

private:
    float x_ = 0.f;
    float y_ = 0.f;
};
