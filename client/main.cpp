#include "client.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

namespace {
sf::Color color_for(const CellView& cell, std::uint32_t self_id) {
    if (cell.kind == protocol::kKindFood) {
        return sf::Color(120, 220, 90);
    }
    if (cell.id == self_id) {
        return sf::Color(80, 160, 255);
    }
    return sf::Color(240, 120, 90);
}
}  // namespace

// Тема: графический поток только наблюдает снимок.
//
// SFML живёт в main. Сеть живёт в io_context на втором потоке. Общего мира
// Entity здесь нет: клиент не владеет клетками сервера, он рисует CellView,
// то есть значения, скопированные из UDP. Камера следует за своим id.
// Окно не является условием сетевого контракта — байты те же, что в tests
// и в server. Рисование можно выкинуть, протокол останется.
int main() {
    try {
        net::io_context io;
        auto client = std::make_shared<GameClient>(io, "127.0.0.1", 9002, 9003);
        client->start();

        // work-guard не нужен: у клиента всегда висит async_read или connect.
        // Поток крутит run, пока main не вызовет stop и io.stop.
        std::thread net_thread([&io] { io.run(); });

        sf::RenderWindow window(sf::VideoMode(sf::Vector2u(960, 720)), "agar");
        window.setFramerateLimit(60);
        sf::View view(sf::FloatRect(sf::Vector2f(-480.f, -360.f), sf::Vector2f(960.f, 720.f)));

        // SFML 3 не рисует текст без шрифта. Системный файл Windows — наблюдаемый
        // интерфейс, а не часть игрового протокола.
        sf::Font font;
        const bool has_font = font.openFromFile("C:/Windows/Fonts/segoeui.ttf");

        while (window.isOpen()) {
            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                }
            }

            const sf::Vector2i mouse = sf::Mouse::getPosition(window);
            const sf::Vector2f center(window.getSize().x * 0.5f, window.getSize().y * 0.5f);
            client->set_direction(Vector2{
                static_cast<float>(mouse.x) - center.x,
                static_cast<float>(mouse.y) - center.y,
            });

            const auto cells = client->cells();
            const std::uint32_t self_id = client->player_id();
            sf::Vector2f camera(0.f, 0.f);
            for (const CellView& cell : cells) {
                if (cell.id == self_id && cell.kind == protocol::kKindPlayer) {
                    camera = sf::Vector2f(cell.x, cell.y);
                }
            }
            view.setCenter(camera);
            window.setView(view);
            window.clear(sf::Color(28, 36, 48));

            // Сетка видна и до первого UDP-снимка: пустой мир больше не выглядит
            // как неоткрывшееся окно.
            for (int i = -4; i <= 4; ++i) {
                const float x = camera.x + static_cast<float>(i) * 120.f;
                const float y = camera.y + static_cast<float>(i) * 120.f;
                sf::RectangleShape vertical(sf::Vector2f(1.f, 960.f));
                vertical.setPosition(sf::Vector2f(x, camera.y - 480.f));
                vertical.setFillColor(sf::Color(50, 64, 82));
                window.draw(vertical);
                sf::RectangleShape horizontal(sf::Vector2f(960.f, 1.f));
                horizontal.setPosition(sf::Vector2f(camera.x - 480.f, y));
                horizontal.setFillColor(sf::Color(50, 64, 82));
                window.draw(horizontal);
            }

            for (const CellView& cell : cells) {
                sf::CircleShape shape(std::max(cell.radius, 4.f));
                shape.setOrigin(sf::Vector2f(shape.getRadius(), shape.getRadius()));
                shape.setPosition(sf::Vector2f(cell.x, cell.y));
                shape.setFillColor(color_for(cell, self_id));
                window.draw(shape);
            }

            window.setView(window.getDefaultView());
            sf::RectangleShape panel(sf::Vector2f(960.f, 72.f));
            panel.setFillColor(sf::Color(12, 16, 24, 220));
            window.draw(panel);

            if (has_font) {
                const int link = client->link_state();
                std::string status = "Нет связи с 127.0.0.1:9002. Сначала запустите agar_server.";
                if (link == 2) {
                    status = "Сервер не принял TCP. Запустите agar_server и откройте клиент снова.";
                } else if (link == 1 && self_id == 0) {
                    status = "TCP есть, ждём Welcome с номером игрока.";
                } else if (link == 1 && cells.empty()) {
                    status = "Игрок " + std::to_string(self_id) + ". Снимок мира по UDP ещё не пришёл.";
                } else if (link == 1) {
                    status = "Игрок " + std::to_string(self_id) + "    клетки: " + std::to_string(cells.size()) +
                             "    мышь задаёт направление";
                }
                // Конструктор sf::String от std::string читает строку как ANSI
                // текущей локали, не как UTF-8. Флаг /utf-8 кладёт в литерал
                // байты UTF-8, поэтому в SFML их нужно передать через fromUtf8.
                const sf::String line = sf::String::fromUtf8(status.begin(), status.end());
                sf::Text text(font, line, 20);
                text.setFillColor(sf::Color(230, 236, 245));
                text.setPosition(sf::Vector2f(16.f, 22.f));
                window.draw(text);
            }
            window.display();
        }

        // Сначала останавливаем цикл, присоединяем поток и только потом
        // закрываем сокеты. close с потока окна одновременно с run() был бы
        // гонкой по сокету. После join сетевой поток сокеты не трогает.
        io.stop();
        net_thread.join();
        client->stop();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
    return 0;
}
