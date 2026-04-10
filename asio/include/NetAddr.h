#pragma once

#include <optional>
#include <string>
#include <tuple>

namespace asio {

/**
 * @brief 网络地址结构体 (IP + 端口)
 * @details 
 * 继承自 std::tuple，提供网络地址的紧凑表示和便捷操作。
 * 支持从字符串解析、哈希、排序等功能。
 * 
 * @note 
 * - 使用 operator!() 判断地址是否无效
 * - 支持 std::optional 用于解析结果
 * - 可用于 std::unordered_map/std::set 等容器
 */
struct NetAddr : std::tuple<std::string, uint16_t>
{
    using std::tuple<std::string, uint16_t>::tuple;

    /// @brief 获取IP地址
    /// @return IP地址引用
    const std::string& ip() const noexcept { return std::get<0>(*this); }

    /// @brief 获取端口号
    /// @return 端口号
    uint16_t port() const noexcept { return std::get<1>(*this); }

    /// @brief 转换为字符串形式
    /// @return 格式为 "ip:port" 的字符串
    std::string toString() const { return ip() + ":" + std::to_string(port()); }

    /// @brief 检查地址是否无效
    /// @return 若IP为空或端口为0则返回true
    bool operator!() const noexcept { return ip().empty() || port() == 0; }

    /**
     * @brief 从字符串解析网络地址
     * @param addrStr 地址字符串，格式为 "ip:port"
     * @return 
     * - 解析成功返回有效的NetAddr
     * - 格式错误或端口为0返回 std::nullopt
     * @throws 不抛异常，错误返回std::nullopt
     * 
     * @example
     * @code
     * auto addr = NetAddr::parse("127.0.0.1:8080");
     * if (addr) {
     *     std::cout << addr->toString();
     * }
     * @endcode
     */
    static std::optional<NetAddr> parse(const std::string& addrStr) noexcept
    {
        auto pos = addrStr.find(':');
        if (pos == std::string::npos || pos == 0 || pos == addrStr.length() - 1) {
            return std::nullopt;
        }
        try {
            auto port = static_cast<uint16_t>(std::stoi(addrStr.substr(pos + 1)));
            return port ? std::make_optional<NetAddr>(addrStr.substr(0, pos), port) : std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    /// @brief 计算哈希值
    /// @return 适用于哈希容器的哈希值
    size_t hash() const noexcept
    {
        return std::hash<std::string>()(ip()) ^ (std::hash<uint16_t>()(port()) << 1);
    }

    /// @brief 比较操作符（用于排序）
    /// @param other 另一个地址
    /// @return 若当前地址应在other之前返回true
    bool operator<(const NetAddr& other) const noexcept
    {
        return (ip() != other.ip()) ? (ip() < other.ip()) : (port() < other.port());
    }
};

} // namespace asio

/// @brief std::hash 特化，支持NetAddr用于无序容器
template<>
struct std::hash<asio::NetAddr>
{
    /// @brief 计算NetAddr的哈希值
    /// @param addr 要哈希的网络地址
    /// @return 哈希值
    size_t operator()(const asio::NetAddr& addr) const noexcept { return addr.hash(); }
};