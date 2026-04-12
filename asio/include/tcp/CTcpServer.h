#pragma once

#include "NetDefs.h"
#include <memory>

#ifdef NETWORK_EXPORTS
#define CTCP_SERVER_API __declspec(dllexport)
#else
#define CTCP_SERVER_API __declspec(dllimport)
#endif

namespace asio {

class CTCP_SERVER_API CTcpServer
{
public:
    CTcpServer();
    ~CTcpServer();
    CTcpServer(const CTcpServer&) = delete;
    CTcpServer& operator=(const CTcpServer&) = delete;

public:
    /**
     * @brief 初始化TCP服务器
     * @param threads 线程数量，默认值为0表示使用系统默认线程数
     * @return 初始化成功返回true，否则返回false
     */
    bool init(size_t threads = 0) noexcept;
    /**
     * @brief 开始监听指定端口
     * @param port 要监听的端口号
     */
    bool listen(uint16_t port) noexcept;
    /**
     * @brief 停止TCP服务器
     */
    void stop() noexcept;

    bool sendToClient(const NetAddr& clientAddr, std::string_view data) noexcept;
    bool sendToClient(const NetAddr& clientAddr, const std::string& data) noexcept;
    bool sendToClient(const NetAddr& clientAddr, const char* data, size_t length) noexcept;

    bool broadcast(std::string_view data) noexcept;
    bool broadcast(const std::string& data) noexcept;
    bool broadcast(std::string&& data) noexcept;
    bool broadcast(const char* data, size_t length) noexcept;

    void disconnect(const NetAddr& clientAddr) noexcept;

    /**
     * @brief 设置服务器回调
     * @param callback 回调函数对象
     */
    void setCallback(ServerCallback&& callback) noexcept;
    /**
     * @brief 设置服务器回调
     * @param callback 回调函数对象
     */
    void setCallback(const ServerCallback& callback) noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> m_pImpl;
};

} // namespace asio