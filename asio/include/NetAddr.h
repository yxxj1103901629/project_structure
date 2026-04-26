#pragma once

#include <optional>
#include <string>

namespace asio {

/**
 * @brief 网络地址结构体 (IP + 端口)
 * @details 提供网络地址的紧凑表示和便捷操作。
 * 支持从字符串解析、哈希、排序等功能。
 *
 * @note
 * - 使用 operator!() 判断地址是否无效
 * - 支持 std::optional 用于解析结果
 * - 可用于 std::unordered_map/std::set 等容器
 */
struct NetAddr
{
    std::string ip;
    uint16_t port{0};

    NetAddr() = default;
    NetAddr(std::string ip_, uint16_t port_) : ip(std::move(ip_)), port(port_) {}

    /// @brief 转换为字符串形式
    /// @return 格式为 "ip:port" 的字符串
    std::string toString() const { return ip + ":" + std::to_string(port); }

    /// @brief 检查地址是否无效
    /// @return 若IP为空或端口为0则返回true
    bool operator!() const noexcept { return ip.empty() || port == 0; }

    /**
     * @brief 从字符串解析网络地址
     * @param addrStr 地址字符串，格式为 "ip:port"
     * @return
     * - 解析成功返回有效的NetAddr
     * - 格式错误或端口为0返回 std::nullopt
     * @throws 不抛异常，错误返回std::nullopt
     */
    static std::optional<NetAddr> parse(const std::string& addrStr) noexcept
    {
        auto pos = addrStr.find(':');
        if (pos == std::string::npos || pos == 0 || pos == addrStr.length() - 1) {
            return std::nullopt;
        }
        try {
            auto p = static_cast<uint16_t>(std::stoi(addrStr.substr(pos + 1)));
            return p ? std::make_optional<NetAddr>(addrStr.substr(0, pos), p) : std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    /// @brief 计算哈希值
    size_t hash() const noexcept
    {
        return std::hash<std::string>()(ip) ^ (std::hash<uint16_t>()(port) << 1);
    }

    /// @brief 相等比较（用于 unordered_map 查找）
    bool operator==(const NetAddr& other) const noexcept
    {
        return ip == other.ip && port == other.port;
    }

    /// @brief 比较操作符（用于排序容器）
    bool operator<(const NetAddr& other) const noexcept
    {
        return (ip != other.ip) ? (ip < other.ip) : (port < other.port);
    }
};

} // namespace asio

/// @brief std::hash 特化，支持NetAddr用于无序容器
template<>
struct std::hash<asio::NetAddr>
{
    size_t operator()(const asio::NetAddr& addr) const noexcept { return addr.hash(); }
};
