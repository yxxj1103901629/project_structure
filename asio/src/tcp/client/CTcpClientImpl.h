#pragma once

#include "tcp/CTcpClient.h"
#include "utils/AsioThreadPool.h"

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <concurrentqueue-1.0.4/concurrentqueue.h>
#include <mutex>

namespace {

constexpr auto MAX_READ_LENGTH = 4096;

}

namespace asio {

enum class ClientState : uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2
};

class CTcpClient::Impl : public std::enable_shared_from_this<CTcpClient::Impl>
{
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Threads = ThreadGroup;

public:
    Impl();
    ~Impl();

    bool init() noexcept;
    bool connect(const NetAddr& serverAddr) noexcept;
    void disconnect() noexcept;
    bool send(std::string_view data) noexcept;
    void setCallback(const ClientCallback& callback) noexcept;
    void setCallback(ClientCallback&& callback) noexcept;

private:
    void doConnect() noexcept;
    void onConnect(const boost::system::error_code& ec) noexcept;
    void doDisconnect(bool isRetry = true) noexcept;
    void doRead() noexcept;
    void onRead(const boost::system::error_code& ec, size_t len) noexcept;
    void doWrite() noexcept;
    void onWrite(const boost::system::error_code& ec, size_t len) noexcept;

    void reportConnected() noexcept;
    void reportDisconnected() noexcept;
    void reportDataReceived(const char* data, size_t length) noexcept;
    void reportError(std::string msg) noexcept;
    void closeSocket() noexcept;

    void postIo(std::function<void(Impl&)> task) noexcept;
    void postTask(std::function<void(Impl&)> task) noexcept;
    bool initializeContext(size_t threadCount,
                           boost::asio::io_context& context,
                           WorkGuardPtr& workGuard,
                           Threads& threads) noexcept;
    ClientCallback copyCallback() const noexcept;
    void shutdownContexts() noexcept;

private:
    std::atomic<bool> m_initialized{false};

    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_reconnectTimer;
    WorkGuardPtr m_ioWorkGuard;
    Threads m_ioThreads;
    Strand m_ioStrand;
    std::array<char, MAX_READ_LENGTH> m_readBuffer{};
    moodycamel::ConcurrentQueue<std::string> m_sendQueue;
    std::vector<std::string> m_batchMessages;
    std::vector<boost::asio::const_buffer> m_batchViews;

    boost::asio::io_context m_taskContext;
    Strand m_taskStrand;
    WorkGuardPtr m_taskWorkGuard;
    Threads m_taskThreads;

    std::atomic<ClientState> m_connectionState{ClientState::Disconnected};
    std::atomic<bool> m_isWriting{false};

    NetAddr m_connectTarget;
    size_t m_reconnectAttempt{0};

    ClientCallback m_callback;
    mutable std::mutex m_callbackMutex;
};

} // namespace asio
