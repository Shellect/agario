#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// Детерминированный разброс еды без глобального генератора в заголовке.
// rand остаётся деталью cpp: тесты задают координаты игрока сами и не
// зависят от последовательности спавна, если проверяют только пересечение
// в одной точке.
float rand_coord() {
    return static_cast<float>(std::rand() % 2000) - 1000.f;
}

}  // namespace

std::uint32_t World::add_player(Vector2 pos, float radius) {
    const std::uint32_t id = next_id_++;
    // make_unique и сразу владение. Промежуточного голого указателя нет.
    entities_.push_back(std::make_unique<Player>(id, pos, radius));
    return id;
}

void World::remove_player(std::uint32_t id) {
    // erase уничтожает unique_ptr, тот вызывает виртуальный деструктор.
    // Игрок после этого не существует, даже если TCP-колбэк ещё «помниет» id.
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                        [id](const std::unique_ptr<Entity>& entity) {
                            auto* player = dynamic_cast<Player*>(entity.get());
                            return player != nullptr && player->id() == id;
                        }),
        entities_.end());
}

Player* World::find_player(std::uint32_t id) {
    for (auto& entity : entities_) {
        auto* player = dynamic_cast<Player*>(entity.get());
        if (player != nullptr && player->id() == id) {
            return player;
        }
    }
    return nullptr;
}

void World::set_direction(std::uint32_t id, Vector2 dir) {
    if (Player* player = find_player(id)) {
        player->set_direction(dir);
    }
}

void World::spawn_food_for_test(Vector2 pos) {
    entities_.push_back(std::make_unique<Food>(pos));
}

void World::spawn_food_if_needed() {
    const std::size_t food_count = static_cast<std::size_t>(std::count_if(
        entities_.begin(), entities_.end(),
        [](const std::unique_ptr<Entity>& entity) {
            return dynamic_cast<Food*>(entity.get()) != nullptr;
        }));
    if (food_count >= 40) {
        return;
    }
    entities_.push_back(std::make_unique<Food>(Vector2{rand_coord(), rand_coord()}));
}

void World::eat() {
    // Сначала только помечаем наблюдателей. Удалять из vector в середине
    // обхода нельзя: erase сдвигает хвост и ломает ссылки на элементы.
    // Сырые указатели здесь живы ровно до erase ниже — vector ещё не трогали.
    std::vector<Entity*> dead;
    for (auto& entity : entities_) {
        auto* player = dynamic_cast<Player*>(entity.get());
        if (player == nullptr) {
            continue;
        }
        for (auto& other : entities_) {
            if (other.get() == player) {
                continue;
            }
            if (!player->intersects(*other)) {
                continue;
            }
            if (dynamic_cast<Food*>(other.get()) != nullptr) {
                player->grow(1.f);
                dead.push_back(other.get());
                continue;
            }
            auto* prey = dynamic_cast<Player*>(other.get());
            if (prey != nullptr && player->radius() > prey->radius() * 1.15f) {
                player->grow(prey->radius() * 0.25f);
                dead.push_back(prey);
            }
        }
    }

    if (dead.empty()) {
        return;
    }
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                        [&dead](const std::unique_ptr<Entity>& entity) {
                            return std::find(dead.begin(), dead.end(), entity.get()) != dead.end();
                        }),
        entities_.end());
}

void World::tick(float dt) {
    // Сначала все клетки смотрят на текущий состав, потом мир меняет контейнер.
    // update еды пустой, update игрока двигает позицию. Виртуальный вызов
    // выбирает функцию по объекту в куче, тип указателя в цикле один — Entity*.
    for (auto& entity : entities_) {
        entity->update(dt);
        if (auto* player = dynamic_cast<Player*>(entity.get())) {
            // Направление не сбрасываем: упёршийся в край остаётся прижатым,
            // пока ввод смотрит наружу. Меняется только координата.
            player->set_position(Vector2{
                std::clamp(player->position().x(), -kMap, kMap),
                std::clamp(player->position().y(), -kMap, kMap),
            });
        }
    }
    eat();

    food_accum_ += dt;
    if (food_accum_ >= 0.4f) {
        food_accum_ = 0.f;
        spawn_food_if_needed();
    }
}

std::vector<CellView> World::snapshot() const {
    // const-метод: снимок не двигает мир. Динамическое приведение к Player*
    // из константного unique_ptr даёт const Player*, для id() этого хватает.
    std::vector<CellView> out;
    out.reserve(entities_.size());
    for (const auto& entity : entities_) {
        CellView cell;
        cell.x = entity->position().x();
        cell.y = entity->position().y();
        cell.radius = entity->radius();
        if (const auto* player = dynamic_cast<const Player*>(entity.get())) {
            cell.id = player->id();
            cell.kind = 1;
        } else {
            cell.kind = 2;
        }
        out.push_back(cell);
    }
    return out;
}
