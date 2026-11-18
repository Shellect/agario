#pragma once
// Пара 7–8. Фрагмент для разбора рядом с server/tcp_session.h из репозитория.
// Это не замена их класса, а контракт, который нужно перенести в него.

#include <memory>

class TcpSession;  // их класс

// 1. Копирование удалить. Сессия — ресурс (сокет), не значение (Vector2).
//    TcpSession(const TcpSession&) = delete;
//    TcpSession& operator=(const TcpSession&) = delete;
//    TcpSession(TcpSession&&) = delete;
//    TcpSession& operator=(TcpSession&&) = delete;

// 2. Создавать только так, иначе shared_from_this в start() — ошибка:
//    auto session = std::make_shared<TcpSession>(std::move(socket), udp);
//    session->start();

// 3. Колбэк async_read уже захватывает self. Так и оставить.
//    Не заменять на [&] и не вызывать shared_from_this() из конструктора.

// 4. uint8_t out[4] в колбэке тела неинициализирован, net::write синхронный.
//    Ответ клиенту: поле или shared_ptr на буфер + async_write с тем же self.

// 5. Номера типов в tcp_session.cpp (1 и 2) не совпадают с enum в protocol.h
//    (MSG_JOIN = 0, MSG_INPUT = 1, MSG_LEAVE = 2). Сверять с protocol::TcpMsgType.
