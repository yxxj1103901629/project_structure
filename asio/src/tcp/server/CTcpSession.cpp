#include "CTcpSession.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

using namespace boost::asio;
using namespace boost::system;
using namespace boost::asio::error;

namespace {

constexpr size_t WRITE_BUFFER_BATCH = 16; // 每次批量发送的最大消息数量

}

namespace asio {

CTcpSession::CTcpSession(tcp::socket socket, io_context& io, ISessionObserver* observer)
    : m_socket(std::move(socket))
    , m_readStrand(make_strand(io))
    , m_writeStrand(make_strand(io))
    , m_observer(observer)
{}

CTcpSession::~CTcpSession()
{
    close(); // 确保连接关闭，释放资源
}

bool CTcpSession::start() noexcept
{
    if (!CAtomicUtil::load(m_isConnected)) {
        return false; // 已经关闭，无法启动
    }

    try {
        const auto& endpoint = m_socket.remote_endpoint();
        m_clientAddr = NetAddr{endpoint.address().to_string(), endpoint.port()};
        m_writeBuffer.resize(WRITE_BUFFER_BATCH);
        m_sendBuffers.reserve(WRITE_BUFFER_BATCH);
    } catch (const std::exception& ex) {
        reportError("启动会话失败: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("启动会话发生未知错误");
        return false;
    }

    doRead(); // 启动读取流程，等待客户端发送数据
    return true;
}

void CTcpSession::send(std::string_view data) noexcept
{
    if (data.empty()) {
        return; // 不发送空消息
    }

    if (!CAtomicUtil::load(m_isConnected)) {
        return; // 连接已关闭，无法发送
    }

    m_msgQueue.enqueue(std::string(data)); // 拷贝入队

    dispatch(m_writeStrand, [weak = make_weak_noexcept(shared_from_this())]() {
        auto self = weak.lock();
        // 如果当前没有正在发送的消息，启动发送流程
        if (self && !CAtomicUtil::exchange(self->m_isWriting, true)) {
            self->doWrite(); // 启动发送流程
        }
    });
}

void CTcpSession::send(std::string&& data) noexcept
{
    if (data.empty()) {
        return; // 不发送空消息
    }

    if (!CAtomicUtil::load(m_isConnected)) {
        return; // 连接已关闭，无法发送
    }

    m_msgQueue.enqueue(std::move(data)); // 将消息添加到发送队列

    dispatch(m_writeStrand, [weak = make_weak_noexcept(shared_from_this())]() {
        auto self = weak.lock();
        // 如果当前没有正在发送的消息，启动发送流程
        if (self && !CAtomicUtil::exchange(self->m_isWriting, true)) {
            self->doWrite(); // 启动发送流程
        }
    });
}

void CTcpSession::close() noexcept
{
    if (!CAtomicUtil::exchange(m_isConnected, false)) {
        return; // 已经关闭，无需重复操作
    }

    // 通过串行化执行器异步关闭连接，确保线程安全
    dispatch(m_writeStrand, [weak = make_weak_noexcept(shared_from_this())]() {
        auto self = weak.lock();
        if (!self)
            return;
        error_code ec;
        // 先关闭发送和接收，然后关闭套接字
        [[maybe_unused]] auto se = self->m_socket.shutdown(tcp::socket::shutdown_both, ec);
        [[maybe_unused]] auto ce = self->m_socket.close(ec);
    });
}

void CTcpSession::doRead() noexcept
{
    if (!CAtomicUtil::load(m_isConnected)) {
        return; // 连接已关闭，不再执行读取操作
    }

    // 弱指针捕获 self：socket.close() 取消 I/O 发生在 ~Impl/~CTcpSession 内，
    // 回调执行时对象尚存，lock() 必定成功，buffer 安全。
    auto weak = make_weak_noexcept(shared_from_this());
    auto onRead = [weak](error_code ec, std::size_t len) {
        auto self = weak.lock();
        if (!self)
            return;
        if (!ec && len > 0) { // 成功读取数据
            // 成功读取数据，触发消息回调并继续读取下一个数据包
            self->reportMessage(self->m_readBuffer.data(), len);
            self->doRead();
            return;
        }

        // 由 close() 主动取消的操作，不触发断开回调
        if (ec == operation_aborted) {
            return;
        }

        if (ec && ec != eof && ec != connection_reset && ec != connection_aborted) {
            // 发生非正常断开以外的错误，报告错误信息
            self->reportError("读取数据失败: " + ec.message());
        }

        // 触发断开回调并关闭连接
        self->reportDisconnect();
        self->close();
    };

    // 异步读取数据到 m_readBuffer，将完成回调绑定到 m_readStrand
    // 读写使用独立 strand，允许读写并发；socket.close() 会原子取消所有待定 I/O
    m_socket.async_read_some(boost::asio::buffer(m_readBuffer), bind_executor(m_readStrand, onRead));
}

void CTcpSession::doWrite() noexcept
{
    if (!CAtomicUtil::load(m_isConnected)) {
        CAtomicUtil::store(m_isWriting, false);
        return;
    }

    const auto count = m_msgQueue.try_dequeue_bulk(m_writeBuffer.data(), WRITE_BUFFER_BATCH);
    if (count == 0) {
        CAtomicUtil::store(m_isWriting, false);
        return;
    }

    m_sendBuffers.clear();
    for (size_t i = 0; i < count; ++i) {
        m_sendBuffers.emplace_back(boost::asio::buffer(m_writeBuffer[i]));
    }

    // 弱指针捕获：close() 取消写操作发生在对象析构内，回调时 buffer 有效
    auto weak = make_weak_noexcept(shared_from_this());
    async_write(m_socket,
                m_sendBuffers,
                bind_executor(m_writeStrand, [weak](error_code ec, std::size_t) {
                    auto self = weak.lock();
                    if (!self)
                        return;
                    if (ec) {
                        CAtomicUtil::store(self->m_isWriting, false);
                        self->reportError("发送数据失败: " + ec.message());
                        self->close();
                    } else {
                        self->doWrite();
                    }
                }));
}

void CTcpSession::reportError(const std::string& msg) noexcept
{
    if (m_observer)
        m_observer->onSessionError(msg);
}

void CTcpSession::reportMessage(const char* data, size_t length) noexcept
{
    if (m_observer)
        m_observer->onSessionMessage(m_clientAddr, {data, length});
}

void CTcpSession::reportDisconnect() noexcept
{
    if (m_observer)
        m_observer->onSessionDisconnected(m_clientAddr);
}

} // namespace asio