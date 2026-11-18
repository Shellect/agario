#include "world.hpp"

#include <catch2/catch_test_macros.hpp>

// Тема: тест смотрит на наблюдаемый результат владения, не на сокет и не
// на dynamic_cast. Мир на стеке. В конце теста деструктор vector уничтожает
// unique_ptr, те — виртуальные деструкторы клеток. Ручного delete нет.

TEST_CASE("far food stays in the world") {
    World world;
    world.add_player(Vector2{0.f, 0.f}, 10.f);
    world.spawn_food_for_test(Vector2{400.f, 400.f});
    world.spawn_food_for_test(Vector2{-400.f, 50.f});
    world.tick(0.016f);
    REQUIRE(world.size() == 3);
}

TEST_CASE("player eats overlapping food and grows") {
    World world;
    world.add_player(Vector2{0.f, 0.f}, 20.f);
    world.spawn_food_for_test(Vector2{0.f, 0.f});
    world.tick(0.016f);
    REQUIRE(world.size() == 1);
    const auto snap = world.snapshot();
    REQUIRE(snap.size() == 1);
    REQUIRE(snap[0].kind == 1);
    REQUIRE(snap[0].radius > 20.f);
}

TEST_CASE("empty world tick is safe") {
    World world;
    world.tick(0.016f);
    REQUIRE(world.size() == 0);
}
