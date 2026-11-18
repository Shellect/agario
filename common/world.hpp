#pragma once

#include "food.hpp"
#include "player.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Снимок одной клетки для сети и для отрисовки.
// Это значение: его копируют в UDP-датаграмму и в клиентский кадр.
// В снимке нет указателей на живые Entity. Клиент не владеет миром сервера
// и не должен держать Entity* дольше кадра сервера.
struct CellView {
    std::uint32_t id = 0;
    float x = 0.f;
    float y = 0.f;
    float radius = 0.f;
    std::uint8_t kind = 0; // 1 — игрок, 2 — еда
};

// Тема: композиция и единственный владелец.
//
// World содержит vector<unique_ptr<Entity>>. Уникальный указатель нельзя
// скопировать, его можно только переместить, поэтому у каждой клетки один
// хозяин. Создание идёт через make_unique: выделение и заворот во владение
// — одно выражение. Если push_back бросит исключение, локальный unique_ptr
// уничтожится при раскрутке стека и освободит память. Голый new в этой щели
// теряет объект. Поэтому в common нет new/delete.
//
// Сырой Entity*, который eat() берёт на время цикла, — наблюдатель.
// Он не переживает erase и не хранится в полях. Долгоживущее имя игрока
// в сети — целое id. Ссылка на Player из add_player действительна только
// до ближайшего удаления или переаллокации, снаружи её не сохраняют.
//
// dynamic_cast в eat — сознательно слабое место модели. Мир спрашивает
// «ты точно игрок?». Новый вид клетки добавляет ветку. Виртуальная реакция
// на столкновение убрала бы ветки, но ей нужен более точный контракт, чем
// один intersects. Тесты проверяют число клеток и радиус, не сам cast:
// реализацию можно заменить, не ломая наблюдаемый результат.
class World {
public:
    static constexpr float kMap = 2000.f;

    std::uint32_t add_player(Vector2 pos, float radius = 18.f);
    void remove_player(std::uint32_t id);
    void set_direction(std::uint32_t id, Vector2 dir);

    void tick(float dt);

    // Только для тестов: положить еду в известную точку, без случайного спавна.
    void spawn_food_for_test(Vector2 pos);

    std::size_t size() const { return entities_.size(); }
    std::vector<CellView> snapshot() const;

private:
    void spawn_food_if_needed();
    void eat();
    Player* find_player(std::uint32_t id);

    std::vector<std::unique_ptr<Entity>> entities_;
    std::uint32_t next_id_ = 1;
    float food_accum_ = 0.f;
};
