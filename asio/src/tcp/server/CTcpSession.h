#pragma once

#include "Defs.h"
#include "boost/asio/io_context.hpp"
#include <boost/asio.hpp>
#include <concurrentqueue-1.0.4/concurrentqueue.h>

namespace asio {

using boost::asio::ip::tcp;

constexpr size_t MAX_MESSAGE_SIZE = 4096; // 最大消息大小，限制单条消息长度

class CTcpSession : public std::enable_shared_from_this<CTcpSession>
{
    /// @brief 串行化执行器，确保同一连接的操作按顺序执行，避免竞争条件
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    /// @brief 消息队列，存储待发送的消息，支持多线程安全访问
    using MsgQueue = moodycamel::ConcurrentQueue<std::string>;

public:
    CTcpSession(tcp::socket socket,
                boost::asio::io_context& io,
                ServerCallback::ErrorCb&& errorcb,
                ServerCallback::MsgCb&& msgcb,
                ServerCallback::AddrCb&& disconnectcb);
    ~CTcpSession();

public:
    const NetAddr& getClientAddr() const noexcept { return m_clientAddr; }

    bool start() noexcept;

    void send(std::string&& data) noexcept;

    void close() noexcept;

private:
    // 异步读写核心方法
    void doRead() noexcept;
    void doWrite() noexcept;

    // 回调触发方法，异步调用应用层回调
    void reportError(const std::string& msg) noexcept;
    void reportMessage(const char* data, size_t length) noexcept;
    void reportDisconnect() noexcept;

private:
    // 连接状态
    std::atomic<bool> m_isConnected{true}; ///< 连接状态标志，表示当前连接是否有效

    // 连接和线程相关
    tcp::socket m_socket; ///< TCP套接字
    NetAddr m_clientAddr; ///< 客户端地址
    Strand m_writeStrand; ///< 发送操作串行化执行器

    // 发送队列及缓冲
    MsgQueue m_msgQueue;                                  ///< 待发送消息队列
    std::atomic<bool> m_isWriting{false};                 ///< 发送状态标志
    std::vector<std::string> m_writeBuffer;               ///< 消息槽位（复用内存）
    std::vector<boost::asio::const_buffer> m_sendBuffers; ///< 发送缓冲序列

    // 接收缓冲
    std::array<char, MAX_MESSAGE_SIZE> m_readBuffer; ///< 读取缓冲区

    // 应用回调
    ServerCallback::ErrorCb m_errorcb = nullptr;
    ServerCallback::MsgCb m_msgcb = nullptr;
    ServerCallback::AddrCb m_disconnectcb = nullptr;
};

} // namespace asio