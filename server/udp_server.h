#pragma once

#include "net.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

using udp = net::ip::udp;

// Тема: ресурс, который нельзя копировать, и RAII вокруг мьютекса.
//
// UdpServer владеет udp-сокетом. Копия сервера скопировала бы право пользоваться
// одним портом дважды, поэтому копирование и перемещение удалены. Жизнь объекта
// продлевает shared_ptr: колбэк приёма держит копию, полученную из
// shared_from_this. Создавать сервер можно только через make_shared, иначе
// shared_from_this в start() — неопределённое поведение (из конструктора его
// вызывать тоже нельзя, управляющий блок ещё не готов).
//
// m_ защищает только карту клиентов. lock_guard захватывает мьютекс в
// конструкторе и отпускает в деструкторе, в том числе при исключении и при
// return. Голых lock/unlock в классе нет. Критическая секция не включает
// async_send_to: под замком копируют адреса наружу, затем замок кончается.
//
// Несколько async_send_to на одном сокете одновременно Asio не обещает.
// Поэтому рассылка сериализована: одна операция, в колбэке следующая.
// Поле sender_ пишет только одна цепочка doReceive (следующий приём ставится
// в конце колбэка). Это не «UDP сам потокобезопасен», это свойство одной
// незавершённой операции. Карта всё равно под мьютексом, потому что в неё
// входит и колбэк UDP, и регистрация с TCP-обработчика. На одном потоке
// io_context гонки нет, замок остаётся как контракт на тот случай, когда
// run() вынесут в пул. strand в этом блоке не вводим: это очередь Asio,
// которая не пускает обработчики параллельно; она нужна вместе с пулом.
class UdpServer : public std::enable_shared_from_this<UdpServer> {
public:
    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;
    UdpServer(UdpServer&&) = delete;
    UdpServer& operator=(UdpServer&&) = delete;

    UdpServer(net::io_context& ioc, std::uint16_t port);

    void start();
    void register_client(std::uint32_t id, udp::endpoint ep);
    void unregister_client(std::uint32_t id);
    void broadcast(std::vector<std::uint8_t> packet);

private:
    void do_receive();
    void pump_send();

    udp::socket sock_;
    udp::endpoint sender_{};
    std::array<std::uint8_t, 64> recv_{};

    // mutable был бы уместен, если бы метод карты был const и при этом брал замок.
    // Сейчас все методы, трогающие clients_, логически меняют или вызываются
    // из неконстантного сервера, поэтому mutex обычное поле.
    std::mutex mutex_;
    std::unordered_map<std::uint32_t, udp::endpoint> clients_;

    bool sending_ = false;
    std::vector<udp::endpoint> send_queue_;
    std::shared_ptr<std::vector<std::uint8_t>> sending_buf_;
};
