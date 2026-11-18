#include "udp_server.h"

#include <cstring>

UdpServer::UdpServer(net::io_context& ioc, std::uint16_t port)
    : sock_(ioc, udp::endpoint(udp::v4(), port)) {}

void UdpServer::start() {
    do_receive();
}

void UdpServer::register_client(std::uint32_t id, udp::endpoint ep) {
    // Эндпоинт принят по значению: в карту кладётся копия, а не ссылка на
    // поле sender_, которое следующая датаграмма перезапишет.
    std::lock_guard<std::mutex> lock(mutex_);
    clients_[id] = ep;
}

void UdpServer::unregister_client(std::uint32_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(id);
}

void UdpServer::broadcast(std::vector<std::uint8_t> packet) {
    std::vector<udp::endpoint> eps;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        eps.reserve(clients_.size());
        for (const auto& [id, ep] : clients_) {
            (void)id;
            eps.push_back(ep);
        }
    }

    auto buf = std::make_shared<std::vector<std::uint8_t>>(std::move(packet));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Новый снимок заменяет ещё не ушедшую очередь. Терять устаревшее
        // состояние для UDP нормально: следующий тик пришлёт свежее.
        send_queue_ = std::move(eps);
        sending_buf_ = std::move(buf);
        if (sending_) {
            return;
        }
        sending_ = true;
    }
    pump_send();
}

void UdpServer::pump_send() {
    udp::endpoint ep;
    std::shared_ptr<std::vector<std::uint8_t>> buf;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (send_queue_.empty() || !sending_buf_) {
            sending_ = false;
            sending_buf_.reset();
            return;
        }
        ep = send_queue_.back();
        send_queue_.pop_back();
        buf = sending_buf_;
    }

    auto self = shared_from_this();
    // buf захвачен по значению (копия shared_ptr). Память снимка жива,
    // пока Asio не вызовет этот колбэк, даже если sending_buf_ уже заменили
    // следующим broadcast. self держит сервер: сокет не закроется раньше.
    sock_.async_send_to(
        net::buffer(*buf), ep,
        [self, buf](error_code /*ec*/, std::size_t /*n*/) {
            self->pump_send();
        });
}

void UdpServer::do_receive() {
    auto self = shared_from_this();
    sock_.async_receive_from(
        net::buffer(recv_), sender_,
        [self](error_code ec, std::size_t n) {
            if (ec == net::error::operation_aborted) {
                return;
            }
            if (ec) {
                self->do_receive();
                return;
            }

            // Клиент шлёт 4 байта своего id, чтобы сервер узнал UDP-адрес.
            // Копируем sender_ в локальную переменную сразу: register_client
            // берёт значение, а не ссылку на поле.
            if (n >= 4) {
                std::uint32_t id = 0;
                std::memcpy(&id, self->recv_.data(), 4);
                const udp::endpoint from = self->sender_;
                self->register_client(id, from);
            }
            self->do_receive();
        });
}
