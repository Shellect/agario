#include "tcp_session.h"

#include "protocol.hpp"

TcpSession::TcpSession(tcp::socket sock, UdpServer& udp, World& world)
    : sock_(std::move(sock)), udp_(udp), world_(world) {}

TcpSession::~TcpSession() {
    // Деструктор не бросает. Снятие игрока с мира и с UDP-карты — обычные
    // контейнерные операции. Если joined_ ложен, игрок мог не успеть
    // создаться (обрыв на первом байте), удалять некого.
    if (joined_) {
        world_.remove_player(player_id_);
        udp_.unregister_client(player_id_);
    }
}

void TcpSession::start() {
    // shared_from_this законен только здесь: объект уже внутри shared_ptr,
    // который собрал make_shared в main. В конструкторе этот вызов был бы
    // неопределённым поведением.
    do_read_header();
}

void TcpSession::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    error_code ec;
    // RAII всё равно закроет сокет в деструкторе. Явный shutdown нужен раньше:
    // колбэк чтения держит shared_ptr, сессия жива, пустой close оставил бы
    // соединение открытым. Повторный вызов гасится флагом closed_.
    sock_.shutdown(tcp::socket::shutdown_both, ec);
    sock_.close(ec);
}

void TcpSession::async_write(std::vector<std::uint8_t> data) {
    if (closed_) {
        return;
    }
    // Одна запись за раз. Второй async_write на тот же сокет, пока первый
    // не завершился, — ошибка использования Asio, не «две параллельные отправки».
    // Если запись уже идёт, новый кадр отбрасываем: для join/welcome этого
    // достаточно, поток ввода ответов не шлёт.
    if (writing_) {
        return;
    }
    outgoing_ = std::move(data);
    writing_ = true;
    auto self = shared_from_this();
    net::async_write(
        sock_, net::buffer(outgoing_),
        [self](error_code ec, std::size_t) {
            self->writing_ = false;
            if (ec) {
                self->close();
            }
        });
}

void TcpSession::do_read_header() {
    if (closed_) {
        return;
    }
    auto self = shared_from_this();
    // Захват self — копия shared_ptr, не [&] и не голый [this].
    // [&] сохранил бы ссылки на локальные переменные этой функции; их не
    // останется к моменту колбэка. [this] скопировал бы адрес без увеличения
    // счётчика: снятая с сервера сессия умерла бы раньше чтения.
    net::async_read(
        sock_, net::buffer(hdr_, 3),
        [self](error_code ec, std::size_t) {
            if (ec) {
                self->close();
                return;
            }
            const std::uint8_t type = self->hdr_[0];
            const std::uint16_t len =
                static_cast<std::uint16_t>(self->hdr_[1] | (self->hdr_[2] << 8));
            // Верхняя граница длины — часть протокола. Иначе клиент может
            // заставить resize выделить огромный буфер одним полем заголовка.
            if (len > protocol::kMaxBody) {
                self->close();
                return;
            }
            self->do_read_body(type, len);
        });
}

void TcpSession::do_read_body(std::uint8_t type, std::uint16_t len) {
    body_.resize(len);
    if (len == 0) {
        handle_message(type);
        do_read_header();
        return;
    }
    auto self = shared_from_this();
    net::async_read(
        sock_, net::buffer(body_),
        [self, type](error_code ec, std::size_t) {
            if (ec) {
                self->close();
                return;
            }
            self->handle_message(type);
            if (!self->closed_) {
                self->do_read_header();
            }
        });
}

void TcpSession::handle_message(std::uint8_t type) {
    using protocol::TcpMsgType;
    const auto msg = static_cast<TcpMsgType>(type);

    if (msg == TcpMsgType::Join && !joined_) {
        player_id_ = world_.add_player(Vector2{0.f, 0.f});
        joined_ = true;
        // Ответ собирает протокол, не неинициализированный uint8_t out[4].
        // Запись асинхронная: поток io_context не стоит внутри синхронного write.
        // Байты лежат в поле outgoing_, его держит сессия, сессию держит self
        // в колбэке async_write.
        async_write(protocol::make_welcome(player_id_));
        return;
    }

    if (msg == TcpMsgType::Input && joined_) {
        if (auto dir = protocol::read_input(body_)) {
            world_.set_direction(player_id_, *dir);
        }
        return;
    }

    if (msg == TcpMsgType::Leave && joined_) {
        close();
    }
}
