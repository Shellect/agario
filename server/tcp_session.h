#pragma once

#include "udp_server.h"
#include "world.hpp"

#include "net.hpp"

#include <cstdint>
#include <memory>
#include <vector>

using tcp = net::ip::tcp;

class World;

// Тема: сессия — ресурс с общим владением на время асинхронной операции.
//
// Копирование удалено: сокет нельзя честно продублировать почленным
// конструктором. Перемещение тоже удалено. Технически сокет Asio перемещается
// (в конструкторе как раз стоит move сокета, принятого у acceptor), но сама
// сессия всегда живёт внутри shared_ptr. vector<TcpSession> по значению
// сдвигал бы объект в другую ячейку памяти и оставлял бы this, уже уехавший
// в старую лямбду. Общее владение (shared_ptr) и почленная копия — разные
// механизмы. Здесь нужен только первый.
//
// udp_ и world_ — ссылки, то есть наблюдатели. Сессия не уничтожает ни сервер
// UDP, ни мир. Они обязаны пережить сессию: в main они объявлены раньше
// io_context::run и живут до выхода из run. Это тот редкий случай, когда
// ссылка в поле законна, потому что время жизни названо вслух.
//
// Буферы hdr_ и body_ — поля. async_read получает их адрес. Колбэк держит
// shared_ptr на сессию, поэтому байты живы до завершения чтения. Локальный
// массив в do_read_header умер бы сразу после return, а чтение завершилось
// бы позже.
class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(const TcpSession&) = delete;
    TcpSession& operator=(const TcpSession&) = delete;
    TcpSession(TcpSession&&) = delete;
    TcpSession& operator=(TcpSession&&) = delete;

    TcpSession(tcp::socket sock, UdpServer& udp, World& world);
    ~TcpSession();

    void start();

private:
    void close();
    void do_read_header();
    void do_read_body(std::uint8_t type, std::uint16_t len);
    void handle_message(std::uint8_t type);
    void async_write(std::vector<std::uint8_t> data);

    tcp::socket sock_;
    UdpServer& udp_;
    World& world_;
    std::uint8_t hdr_[3]{};
    std::vector<std::uint8_t> body_;
    std::vector<std::uint8_t> outgoing_;
    bool writing_ = false;
    bool closed_ = false;
    std::uint32_t player_id_ = 0;
    bool joined_ = false;
};
