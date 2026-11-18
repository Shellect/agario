#include "client.h"

#include <cstring>

GameClient::GameClient(net::io_context& io, const std::string& host,
                       std::uint16_t tcp_port, std::uint16_t udp_port)
    : io_(io),
      tcp_(io),
      udp_(io),
      server_udp_(net::ip::make_address(host), udp_port) {
    tcp::resolver resolver(io);
    tcp_endpoints_ = resolver.resolve(host, std::to_string(tcp_port));
}

void GameClient::start() {
    auto self = shared_from_this();
    net::async_connect(tcp_, tcp_endpoints_, [self](error_code ec, const tcp::endpoint&) {
        if (ec) {
            self->link_state_.store(2);
            return;
        }
        self->link_state_.store(1);
        self->udp_.open(udp::v4());
        // Приём без bind на Windows завершается ошибкой, обработчик выходит
        // и больше не слушает. Сервер шлёт снимок в пустоту: нет ни игрока, ни еды.
        self->udp_.bind(udp::endpoint(udp::v4(), 0));
        self->send_frame(protocol::make_join());
        self->do_read_header();
        self->do_receive_snapshot();
    });
}

void GameClient::stop() {
    stopped_ = true;
    error_code ec;
    tcp_.close(ec);
    udp_.close(ec);
}

void GameClient::set_direction(Vector2 dir) {
    // post переносит работу на поток run(). Копия shared_ptr удерживает клиента,
    // даже если окно уже начало закрываться и локальных ссылок не осталось.
    net::post(io_, [self = shared_from_this(), dir] {
        self->dir_ = dir;
        if (self->player_id_.load() == 0 || self->stopped_) {
            return;
        }
        // Не копим кадры быстрее, чем сокет успевает унести. Лишний ввод
        // заменяется следующим положением мыши, протокол это переживёт.
        if (self->write_queue_.size() < 2) {
            self->send_frame(protocol::make_input(dir));
        }
        // Повторяем адрес, пока одна отправка не наложится на другую.
        // Один потерянный UDP-пакет иначе оставляет карту сервера пустой.
        self->announce_udp();
    });
}

std::vector<CellView> GameClient::cells() const {
    std::lock_guard<std::mutex> lock(cells_mutex_);
    return cells_;
}

void GameClient::send_frame(std::vector<std::uint8_t> data) {
    write_queue_.push_back(std::move(data));
    if (!writing_) {
        pump_write();
    }
}

void GameClient::pump_write() {
    if (write_queue_.empty() || stopped_) {
        writing_ = false;
        return;
    }
    writing_ = true;
    auto self = shared_from_this();
    // Очередь хранит байты в поле, не в локальной string, которая умерла бы
    // до завершения async_write. Захват self удерживает и очередь, и сокет.
    net::async_write(
        tcp_, net::buffer(write_queue_.front()),
        [self](error_code ec, std::size_t) {
            if (ec || self->stopped_) {
                self->writing_ = false;
                return;
            }
            self->write_queue_.erase(self->write_queue_.begin());
            self->pump_write();
        });
}

void GameClient::announce_udp() {
    if (stopped_ || !udp_.is_open()) {
        return;
    }
    const std::uint32_t id = player_id_.load();
    if (id == 0 || udp_busy_) {
        return;
    }
    udp_busy_ = true;
    auto bytes = std::make_shared<std::array<std::uint8_t, 4>>();
    std::memcpy(bytes->data(), &id, 4);
    auto self = shared_from_this();
    udp_.async_send_to(net::buffer(*bytes), server_udp_, [self, bytes](error_code, std::size_t) {
        self->udp_busy_ = false;
    });
}

void GameClient::do_read_header() {
    if (stopped_) {
        return;
    }
    auto self = shared_from_this();
    net::async_read(tcp_, net::buffer(hdr_, 3), [self](error_code ec, std::size_t) {
        if (ec) {
            return;
        }
        const std::uint8_t type = self->hdr_[0];
        const std::uint16_t len = static_cast<std::uint16_t>(self->hdr_[1] | (self->hdr_[2] << 8));
        if (len > protocol::kMaxBody) {
            return;
        }
        self->do_read_body(type, len);
    });
}

void GameClient::do_read_body(std::uint8_t type, std::uint16_t len) {
    body_.resize(len);
    auto finish = [this, type] {
        if (static_cast<protocol::TcpMsgType>(type) == protocol::TcpMsgType::Welcome) {
            if (auto id = protocol::read_welcome(body_)) {
                player_id_.store(*id);
                announce_udp();
            }
        }
        do_read_header();
    };
    if (len == 0) {
        finish();
        return;
    }
    auto self = shared_from_this();
    net::async_read(tcp_, net::buffer(body_), [self, finish](error_code ec, std::size_t) {
        if (ec) {
            return;
        }
        finish();
    });
}

void GameClient::do_receive_snapshot() {
    if (stopped_) {
        return;
    }
    auto self = shared_from_this();
    udp_.async_receive_from(net::buffer(udp_buf_), udp_from_, [self](error_code ec, std::size_t n) {
        if (ec) {
            if (ec == net::error::operation_aborted || self->stopped_) {
                return;
            }
            self->do_receive_snapshot();
            return;
        }
        if (auto snap = protocol::read_snapshot(self->udp_buf_.data(), n)) {
            std::lock_guard<std::mutex> lock(self->cells_mutex_);
            self->cells_ = std::move(*snap);
        }
        self->do_receive_snapshot();
    });
}
