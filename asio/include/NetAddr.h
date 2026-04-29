#pragma once

#include <charconv>
#include <functional>
#include <optional>
#include <string>

namespace asio {

struct NetAddr
{
    std::string ip;
    uint16_t port{0};

    NetAddr() = default;
    NetAddr(std::string ip_, uint16_t port_) : ip(std::move(ip_)), port(port_) {}

    std::string toString() const { return ip + ":" + std::to_string(port); }

    bool operator!() const noexcept { return ip.empty() || port == 0; }

    static std::optional<NetAddr> parse(const std::string& addrStr) noexcept
    {
        const auto pos = addrStr.find(':');
        if (pos == std::string::npos || pos == 0 || pos == addrStr.length() - 1) {
            return std::nullopt;
        }
        if (addrStr.find(':', pos + 1) != std::string::npos) {
            return std::nullopt;
        }

        unsigned int parsedPort = 0;
        const char* begin = addrStr.data() + pos + 1;
        const char* end = addrStr.data() + addrStr.size();
        const auto result = std::from_chars(begin, end, parsedPort);
        if (result.ec != std::errc{} || result.ptr != end || parsedPort == 0 || parsedPort > 65535) {
            return std::nullopt;
        }

        return NetAddr{addrStr.substr(0, pos), static_cast<uint16_t>(parsedPort)};
    }

    size_t hash() const noexcept
    {
        return std::hash<std::string>()(ip) ^ (std::hash<uint16_t>()(port) << 1);
    }

    bool operator==(const NetAddr& other) const noexcept
    {
        return ip == other.ip && port == other.port;
    }

    bool operator<(const NetAddr& other) const noexcept
    {
        return (ip != other.ip) ? (ip < other.ip) : (port < other.port);
    }
};

} // namespace asio

template<>
struct std::hash<asio::NetAddr>
{
    size_t operator()(const asio::NetAddr& addr) const noexcept { return addr.hash(); }
};
