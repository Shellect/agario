#pragma once
#include "food.hpp"
#include "player.hpp"

#include <memory>
#include <utility>
#include <vector>

// Пара 5. Мир владеет сущностями. Снаружи — только unique_ptr, без new/delete.
class World {
public:
    Player& add_player(Vector2 pos, float radius) {
        auto player = std::make_unique<Player>(pos, radius);
        Player& ref = *player;
        entities_.push_back(std::move(player));
        return ref;
    }

    void spawn_food(Vector2 pos) {
        entities_.push_back(std::make_unique<Food>(pos));
    }

    void tick(float dt) {
        for (auto& entity : entities_)
            entity->update(dt);
        eat();
    }

    std::size_t size() const { return entities_.size(); }

private:
    void eat() {
        // Учебный алгоритм: игрок поглощает еду, если круги пересеклись.
        // Индексы, не итераторы: vector перестраивается при erase.
        for (std::size_t i = 0; i < entities_.size(); ++i) {
            auto* player = dynamic_cast<Player*>(entities_[i].get());
            if (!player)
                continue;
            for (std::size_t j = 0; j < entities_.size();) {
                if (i == j) {
                    ++j;
                    continue;
                }
                auto* food = dynamic_cast<Food*>(entities_[j].get());
                if (food && player->intersects(*food)) {
                    player->grow(1.f);
                    entities_.erase(entities_.begin() + static_cast<std::ptrdiff_t>(j));
                    if (j < i)
                        --i;
                } else {
                    ++j;
                }
            }
        }
    }

    std::vector<std::unique_ptr<Entity>> entities_;
};
