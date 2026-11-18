#pragma once

#include "vector2d.hpp"
#include "world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

// Тема: один контракт на клиента и сервер.
//
// Кадр TCP: 1 байт типа, 2 байта длины тела (младший вперёд), затем тело.
// Длина не включает заголовок. Оба конца подключают этот заголовок через
// библиотеку agar_common. Копия констант в server/ и ещё одна в client/
// разъедутся в тот же день — в старом каркасе это уже случилось: enum
// начинался с нуля, а ветки сессии сравнивали тип с литералами 1 и 2.
//
// float по сети уходит машинным представлением через memcpy в локальную
// переменную. Это законно на одном стенде с одинаковой архитектурой клиента
// и сервера. Переносимый порядок байтов и отказ от сырого float — следующий
// блок, не прячем ограничение.
//
// Чтение не начинает memcpy, пока размер тела не проверен. Запись не
// отправляет неинициализированный стековый массив: байты собираются в vector,
// которым владеет вызывающий, а сессия потом кладёт копию в буфер, живущий
// до конца async_write.
namespace protocol {

enum class TcpMsgType : std::uint8_t {
    Join = 0,
    Input = 1,
    Leave = 2,
    Welcome = 3,
};

constexpr std::size_t kHeaderSize = 3;
constexpr std::size_t kWelcomeSize = 4;
constexpr std::size_t kInputSize = 8;
constexpr std::size_t kMaxBody = 512;

constexpr std::uint8_t kKindPlayer = 1;
constexpr std::uint8_t kKindFood = 2;

inline void append_header(std::vector<std::uint8_t>& out, TcpMsgType type, std::uint16_t len) {
    out.push_back(static_cast<std::uint8_t>(type));
    out.push_back(static_cast<std::uint8_t>(len & 0xff));
    out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xff));
}

inline std::vector<std::uint8_t> frame(TcpMsgType type, const std::uint8_t* body, std::uint16_t len) {
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + len);
    append_header(out, type, len);
    if (len != 0 && body != nullptr) {
        out.insert(out.end(), body, body + len);
    }
    return out;
}

inline std::vector<std::uint8_t> make_join() {
    return frame(TcpMsgType::Join, nullptr, 0);
}

inline std::vector<std::uint8_t> make_leave() {
    return frame(TcpMsgType::Leave, nullptr, 0);
}

inline std::vector<std::uint8_t> make_welcome(std::uint32_t id) {
    std::uint8_t body[4] = {
        static_cast<std::uint8_t>(id & 0xff),
        static_cast<std::uint8_t>((id >> 8) & 0xff),
        static_cast<std::uint8_t>((id >> 16) & 0xff),
        static_cast<std::uint8_t>((id >> 24) & 0xff),
    };
    return frame(TcpMsgType::Welcome, body, 4);
}

inline std::vector<std::uint8_t> make_input(Vector2 dir) {
    float coords[2] = {dir.x(), dir.y()};
    auto* bytes = reinterpret_cast<const std::uint8_t*>(coords);
    return frame(TcpMsgType::Input, bytes, static_cast<std::uint16_t>(sizeof(coords)));
}

inline std::optional<std::uint32_t> read_welcome(const std::vector<std::uint8_t>& body) {
    if (body.size() < kWelcomeSize) {
        return std::nullopt;
    }
    std::uint32_t id = static_cast<std::uint32_t>(body[0]) |
                       (static_cast<std::uint32_t>(body[1]) << 8) |
                       (static_cast<std::uint32_t>(body[2]) << 16) |
                       (static_cast<std::uint32_t>(body[3]) << 24);
    return id;
}

inline std::optional<Vector2> read_input(const std::vector<std::uint8_t>& body) {
    if (body.size() < kInputSize) {
        return std::nullopt;
    }
    float x = 0.f;
    float y = 0.f;
    std::memcpy(&x, body.data(), sizeof(float));
    std::memcpy(&y, body.data() + sizeof(float), sizeof(float));
    return Vector2{x, y};
}

// UDP-снимок: uint16 count, затем записи фиксированного размера.
// Отдельный канал от TCP. TCP несёт надёжный ввод и факт входа,
// UDP несёт частое состояние, которое можно потерять.
inline std::vector<std::uint8_t> make_snapshot(const std::vector<CellView>& cells) {
    const std::uint16_t count = static_cast<std::uint16_t>(std::min<std::size_t>(cells.size(), 200));
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(count & 0xff));
    out.push_back(static_cast<std::uint8_t>((count >> 8) & 0xff));
    for (std::uint16_t i = 0; i < count; ++i) {
        const CellView& cell = cells[i];
        const std::uint32_t id = cell.id;
        out.push_back(static_cast<std::uint8_t>(id & 0xff));
        out.push_back(static_cast<std::uint8_t>((id >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>((id >> 16) & 0xff));
        out.push_back(static_cast<std::uint8_t>((id >> 24) & 0xff));
        float parts[3] = {cell.x, cell.y, cell.radius};
        auto* bytes = reinterpret_cast<const std::uint8_t*>(parts);
        out.insert(out.end(), bytes, bytes + sizeof(parts));
        out.push_back(cell.kind);
    }
    return out;
}

inline std::optional<std::vector<CellView>> read_snapshot(const std::uint8_t* data, std::size_t n) {
    constexpr std::size_t kRecord = 4 + sizeof(float) * 3 + 1;
    if (n < 2) {
        return std::nullopt;
    }
    const std::uint16_t count = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    if (n < 2 + static_cast<std::size_t>(count) * kRecord) {
        return std::nullopt;
    }
    std::vector<CellView> cells;
    cells.reserve(count);
    const std::uint8_t* p = data + 2;
    for (std::uint16_t i = 0; i < count; ++i) {
        CellView cell;
        cell.id = static_cast<std::uint32_t>(p[0]) |
                  (static_cast<std::uint32_t>(p[1]) << 8) |
                  (static_cast<std::uint32_t>(p[2]) << 16) |
                  (static_cast<std::uint32_t>(p[3]) << 24);
        p += 4;
        float parts[3] = {};
        std::memcpy(parts, p, sizeof(parts));
        p += sizeof(parts);
        cell.x = parts[0];
        cell.y = parts[1];
        cell.radius = parts[2];
        cell.kind = *p++;
        cells.push_back(cell);
    }
    return cells;
}

}  // namespace protocol
