#pragma once

#include "Defs.h"
#include <memory>

#ifdef NETWORK_EXPORTS
#define CTCP_CLIENT_API __declspec(dllexport)
#else
#define CTCP_CLIENT_API __declspec(dllimport)
#endif

namespace asio {

class CTCP_CLIENT_API CTcpClient
{
public:
    CTcpClient();
    ~CTcpClient();
    CTcpClient(const CTcpClient&) = delete;
    CTcpClient& operator=(const CTcpClient&) = delete;

public:
    /**
     * @brief 初始化TCP客户端
     * @return 初始化成功返回true，否则返回false
     */
    bool init() noexcept;

    /**
     * @brief 连接到服务器
     * @param serverAddr 服务器地址 (IP:Port)
     * @return 连接启动成功返回true，否则返回false
     */
    bool connect(const NetAddr& serverAddr) noexcept;

    /**
     * @brief 断开连接
     */
    void disconnect() noexcept;

    /**
     * @brief 发送数据到服务器
     * @param data 要发送的数据
     * @return 发送成功返回true，否则返回false
     */
    bool send(std::string_view data) noexcept;
    bool send(const std::string& data) noexcept;
    bool send(std::string&& data) noexcept;
    bool send(const char* data, size_t length) noexcept;

    /**
     * @brief 设置客户端回调
     * @param callback 回调函数对象
     */
    void setCallback(ClientCallback&& callback) noexcept;
    /**
     * @brief 设置客户端回调
     * @param callback 回调函数对象
     */
    void setCallback(const ClientCallback& callback) noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> m_pImpl;
};

} // namespace asio
