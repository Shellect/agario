#pragma once

#include "protocol.hpp"
#include "vector2d.hpp"

#include "net.hpp"

#include <array>
#include <string>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

using tcp = net::ip::tcp;
using udp = net::ip::udp;

// Тема: клиент — второй потребитель того же common, что и сервер.
//
// Кадры собирает protocol::make_join / make_input, не локальные магические
// байты. Снимок, пришедший по UDP, кладётся в cells_ под mutex: поток
// io_context пишет, поток SFML читает. Это уже настоящие два потока, в
// отличие от сервера на одном run(). lock_guard ограничивает секцию копией
// вектора. Рисование происходит уже без замка, по локальной копии значения.
//
// Копирование клиента удалено: внутри два сокета. Жизнь на время колбэков
// держит enable_shared_from_this, объект создаётся через make_shared.
class GameClient : public std::enable_shared_from_this<GameClient> {
public:
    GameClient(const GameClient&) = delete;
    GameClient& operator=(const GameClient&) = delete;
    GameClient(GameClient&&) = delete;
    GameClient& operator=(GameClient&&) = delete;

    GameClient(net::io_context& io, const std::string& host, std::uint16_t tcp_port, std::uint16_t udp_port);

    void start();
    void set_direction(Vector2 dir);
    void stop();

    std::vector<CellView> cells() const;
    std::uint32_t player_id() const { return player_id_.load(); }

    // 0 — соединение ещё не установлено, 1 — сервер принял join, 2 — TCP не открылся.
    // Графический поток только читает это слово, пишет его поток io_context.
    int link_state() const { return link_state_.load(); }

private:
    void do_read_header();
    void do_read_body(std::uint8_t type, std::uint16_t len);
    void send_frame(std::vector<std::uint8_t> data);
    void pump_write();
    void announce_udp();
    void do_receive_snapshot();

    // Ссылка на цикл, который переживёт клиента: им владеет main.
    // Сюда графический поток постит ввод, чтобы очередь записи трогал только
    // поток io_context. Прямой send_frame из окна был бы гонкой с pump_write.
    net::io_context& io_;
    tcp::socket tcp_;
    udp::socket udp_;
    udp::endpoint server_udp_;
    tcp::resolver::results_type tcp_endpoints_;

    std::uint8_t hdr_[3]{};
    std::vector<std::uint8_t> body_;
    std::vector<std::vector<std::uint8_t>> write_queue_;
    bool writing_ = false;

    std::array<std::uint8_t, 2048> udp_buf_{};
    bool udp_busy_ = false;
    udp::endpoint udp_from_;

    // player_id_ пишется сетевым потоком и читается графическим.
    // atomic закрывает именно это слово. Вектор клеток так не защитить:
    // ему нужен mutex, снимок больше одного слова.
    std::atomic<std::uint32_t> player_id_{0};
    std::atomic<int> link_state_{0};
    mutable std::mutex cells_mutex_;
    std::vector<CellView> cells_;
    Vector2 dir_{};
    bool stopped_ = false;
};
