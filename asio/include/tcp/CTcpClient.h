#pragma once

#include "NetAddr.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifdef NETWORK_EXPORTS
#define CTCP_CLIENT_API __declspec(dllexport)
#else
#define CTCP_CLIENT_API __declspec(dllimport)
#endif

namespace asio {

struct ClientCallback
{
    using AddrCb = std::function<void(const NetAddr&)>;
    using DataCb = std::function<void(std::string_view)>;
    using ErrorCb = std::function<void(const std::string&)>;

    AddrCb connected = nullptr;
    AddrCb disconnected = nullptr;
    DataCb dataReceived = nullptr;
    ErrorCb errorOccurred = nullptr;
};

class CTCP_CLIENT_API CTcpClient
{
public:
    CTcpClient();
    ~CTcpClient();
    CTcpClient(const CTcpClient&) = delete;
    CTcpClient& operator=(const CTcpClient&) = delete;

    bool init() noexcept;
    bool connect(const NetAddr& serverAddr) noexcept;
    void disconnect() noexcept;

    bool send(std::string_view data) noexcept;
    bool send(const std::string& data) noexcept;
    bool send(const char* data, size_t length) noexcept;

    void setCallback(ClientCallback&& callback) noexcept;
    void setCallback(const ClientCallback& callback) noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> m_pImpl;
};

} // namespace asio
