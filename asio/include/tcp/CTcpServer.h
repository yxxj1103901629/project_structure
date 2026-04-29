#pragma once

#include "NetAddr.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifdef NETWORK_EXPORTS
#define CTCP_SERVER_API __declspec(dllexport)
#else
#define CTCP_SERVER_API __declspec(dllimport)
#endif

namespace asio {

struct ServerCallback
{
    using AddrCb = std::function<void(const NetAddr&)>;
    using DataCb = std::function<void(const NetAddr&, std::string_view)>;
    using ErrorCb = std::function<void(const std::string&)>;

    AddrCb clientConnected = nullptr;
    AddrCb clientDisconnected = nullptr;
    DataCb dataReceived = nullptr;
    ErrorCb errorOccurred = nullptr;
};

class CTCP_SERVER_API CTcpServer
{
public:
    CTcpServer();
    ~CTcpServer();
    CTcpServer(const CTcpServer&) = delete;
    CTcpServer& operator=(const CTcpServer&) = delete;

    bool init(size_t threads = 0) noexcept;
    bool listen(uint16_t port) noexcept;
    void stop() noexcept;

    bool sendToClient(const NetAddr& clientAddr, std::string_view data) noexcept;
    bool sendToClient(const NetAddr& clientAddr, const std::string& data) noexcept;
    bool sendToClient(const NetAddr& clientAddr, const char* data, size_t length) noexcept;

    bool broadcast(std::string_view data) noexcept;
    bool broadcast(const std::string& data) noexcept;
    bool broadcast(std::string&& data) noexcept;
    bool broadcast(const char* data, size_t length) noexcept;

    void disconnect(const NetAddr& clientAddr) noexcept;

    void setCallback(ServerCallback&& callback) noexcept;
    void setCallback(const ServerCallback& callback) noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> m_pImpl;
};

} // namespace asio
