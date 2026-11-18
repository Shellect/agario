#pragma once
#include "vector2d.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

// Пара 9. Тот же формат, что уже заложен в TcpSession: 1 байт типа + 2 байта длины (LE) + тело.
namespace protocol {

enum class TcpMsgType : uint8_t { Join = 0, Input = 1, Leave = 2 };

constexpr std::size_t kHeaderSize = 3;
constexpr std::size_t kInputSize = 8;

inline void append_header(std::vector<uint8_t>& out, TcpMsgType type, uint16_t len) {
    out.push_back(static_cast<uint8_t>(type));
    out.push_back(static_cast<uint8_t>(len & 0xff));
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xff));
}

inline std::vector<uint8_t> make_input(Vector2 dir) {
    std::vector<uint8_t> out;
    out.reserve(kHeaderSize + kInputSize);
    append_header(out, TcpMsgType::Input, static_cast<uint16_t>(kInputSize));
    float x = dir.x();
    float y = dir.y();
    const auto* xb = reinterpret_cast<const uint8_t*>(&x);
    const auto* yb = reinterpret_cast<const uint8_t*>(&y);
    out.insert(out.end(), xb, xb + sizeof(float));
    out.insert(out.end(), yb, yb + sizeof(float));
    return out;
}

inline std::optional<Vector2> read_input(const std::vector<uint8_t>& body) {
    if (body.size() < kInputSize)
        return std::nullopt;
    float x = 0.f;
    float y = 0.f;
    std::memcpy(&x, body.data(), sizeof(float));
    std::memcpy(&y, body.data() + sizeof(float), sizeof(float));
    return Vector2{x, y};
}

}  // namespace protocol
