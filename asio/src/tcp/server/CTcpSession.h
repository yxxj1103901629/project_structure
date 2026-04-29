#pragma once

#include "NetAddr.h"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <concurrentqueue-1.0.4/concurrentqueue.h>

namespace asio {

using boost::asio::ip::tcp;

constexpr size_t MAX_READ_CHUNK_SIZE = 4096;

struct ISessionObserver
{
    virtual void onSessionError(const std::string& msg) noexcept = 0;
    virtual void onSessionData(const NetAddr& addr, std::string_view data) noexcept = 0;
    virtual void onSessionDisconnected(const NetAddr& addr) noexcept = 0;

protected:
    ~ISessionObserver() = default;
};

class CTcpSession : public std::enable_shared_from_this<CTcpSession>
{
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using MsgQueue = moodycamel::ConcurrentQueue<std::string>;

public:
    CTcpSession(tcp::socket socket, boost::asio::io_context& io, ISessionObserver* observer);
    ~CTcpSession();

    const NetAddr& getClientAddr() const noexcept { return m_clientAddr; }

    bool start() noexcept;

    void send(std::string_view data) noexcept;
    void send(std::string&& data) noexcept;

    void close() noexcept;
    void forceClose() noexcept;

private:
    void doRead() noexcept;
    void doWrite() noexcept;
    void closeSocket() noexcept;
    void finishDisconnect() noexcept;

    void reportError(const std::string& msg) noexcept;
    void reportData(const char* data, size_t length) noexcept;
    void reportDisconnect() noexcept;

private:
    std::atomic<bool> m_isConnected{true};
    std::atomic<bool> m_disconnectReported{false};

    tcp::socket m_socket;
    NetAddr m_clientAddr;
    Strand m_readStrand;
    Strand m_writeStrand;

    MsgQueue m_msgQueue;
    std::atomic<bool> m_isWriting{false};
    std::vector<std::string> m_writeBuffer;
    std::vector<boost::asio::const_buffer> m_sendBuffers;

    std::array<char, MAX_READ_CHUNK_SIZE> m_readBuffer{};

    ISessionObserver* m_observer;
};

} // namespace asio
