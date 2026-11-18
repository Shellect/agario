#include "protocol.hpp"
#include "tcp_session.h"
#include "udp_server.h"
#include "world.hpp"

#include "net.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <cstdlib>
#include <iostream>
#include <memory>

using tcp = net::ip::tcp;

// Тема: когда захват [&] у лямбды Asio всё-таки законен.
//
// do_accept и on_tick ссылаются на объекты из main: acceptor, timer, world,
// udp, io. Эти объекты живут весь io_context::run(), а run() не возвращается,
// пока крутится сервер. Колбэк accept вызывается из run, то есть строго
// раньше уничтожения локальных переменных main. Это другое время, чем у
// TcpSession::do_read_header: та функция возвращается сразу, а чтение
// завершается позже, поэтому сессия обязана захватить shared_ptr.
//
// Мир, UDP и TCP трогаются только с потока, который вызывает run(). Второй
// поток на этот io_context не заведён, мьютекс у мира не нужен. Мьютекс
// остаётся внутри UdpServer как граница карты, если run когда-нибудь
// вынесут в пул. Пока пула нет, гонки между тиком и сессией нет:
// обработчики Asio на одном run не пересекаются.
int main() {
    constexpr std::uint16_t kTcpPort = 9002;
    constexpr std::uint16_t kUdpPort = 9003;

    try {
        net::io_context io;
        World world;
        for (int i = 0; i < 25; ++i) {
            world.spawn_food_for_test(Vector2{
                static_cast<float>(std::rand() % 800) - 400.f,
                static_cast<float>(std::rand() % 800) - 400.f,
            });
        }

        // make_shared, и только потом start. Порядок — предусловие shared_from_this.
        auto udp = std::make_shared<UdpServer>(io, kUdpPort);
        udp->start();

        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), kTcpPort));
        std::function<void()> do_accept;
        do_accept = [&]() {
            acceptor.async_accept([&](error_code ec, tcp::socket sock) {
                if (!ec) {
                    std::make_shared<TcpSession>(std::move(sock), *udp, world)->start();
                }
                do_accept();
            });
        };
        do_accept();

        net::steady_timer timer(io);
        std::function<void()> on_tick;
        on_tick = [&]() {
            timer.expires_after(std::chrono::milliseconds(16));
            timer.async_wait([&](error_code ec) {
                if (ec) {
                    return;
                }
                world.tick(0.016f);
                udp->broadcast(protocol::make_snapshot(world.snapshot()));
                on_tick();
            });
        };
        on_tick();

        std::cout << "agar server tcp " << kTcpPort << " udp " << kUdpPort << '\n';
        // Раньше main возвращался сразу после постановки accept. Операции
        // были поставлены в очередь, которую никто не исполнял.
        io.run();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
    return 0;
}
