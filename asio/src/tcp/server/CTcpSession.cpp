#include "CTcpSession.h"
#include "utils/CAtomicUtil.h"

namespace {

constexpr size_t WRITE_BUFFER_BATCH = 16; // 每次批量发送的最大消息数量

}

namespace asio {

CTcpSession::CTcpSession(tcp::socket socket,
                         boost::asio::io_context& io,
                         ServerCallback::ErrorCb&& errorcb,
                         ServerCallback::MsgCb&& msgcb,
                         ServerCallback::AddrCb&& disconnectcb)
    : m_socket(std::move(socket))
    , m_writeStrand(boost::asio::make_strand(io))
    , m_errorcb(std::move(errorcb))
    , m_msgcb(std::move(msgcb))
    , m_disconnectcb(std::move(disconnectcb))
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
    } catch (const std::exception& ex) {
        reportError("获取客户端地址失败: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("获取客户端地址发生未知错误");
        return false;
    }

    try {
        m_writeBuffer.resize(WRITE_BUFFER_BATCH);  // 一次性构造好槽位，后续不再析构/构造
        m_sendBuffers.reserve(WRITE_BUFFER_BATCH); // 预分配 const_buffer 序列容量
    } catch (const std::exception& ex) {
        reportError("初始化发送缓冲失败: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("初始化发送缓冲发生未知错误");
        return false;
    }

    doRead(); // 启动读取流程，等待客户端发送数据
    return true;
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

    boost::asio::post(m_writeStrand, [self = shared_from_this()]() {
        // 如果当前没有正在发送的消息，启动发送流程
        if (!CAtomicUtil::exchange(self->m_isWriting, true)) {
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
    boost::asio::post(m_writeStrand, [self = shared_from_this()]() {
        boost::system::error_code ec;
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

    // 获取shared_ptr以保持对象生命周期
    auto self = shared_from_this();
    // 定义读取完成后的回调函数，处理读取结果
    auto onRead = [self](boost::system::error_code ec, std::size_t len) {
        if (!ec && len > 0) { // 成功读取数据
            // 成功读取数据，触发消息回调并继续读取下一个数据包
            self->reportMessage(self->m_readBuffer.data(), len);
            self->doRead();
            return;
        }

        // 处理错误情况（包括正常关闭和异常错误）
        using namespace boost::asio::error;
        if (ec && ec != eof && ec != connection_reset && ec != connection_aborted) {
            // 发生非正常断开以外的错误，报告错误信息
            self->reportError("读取数据失败: " + ec.message());
        }

        // 触发断开回调并关闭连接
        self->reportDisconnect();
        self->close();
    };

    // 异步读取数据到 m_readBuffer，读取完成后调用 onRead 处理结果
    m_socket.async_read_some(boost::asio::buffer(m_readBuffer), onRead);
}

void CTcpSession::doWrite() noexcept
{
    try {
        // 检查连接状态和消息队列
        if (!CAtomicUtil::load(m_isConnected)) {
            CAtomicUtil::store(m_isWriting, false);
            return;
        }

        const auto& count = m_msgQueue.try_dequeue_bulk(m_writeBuffer.data(), WRITE_BUFFER_BATCH);
        if (count == 0) {
            CAtomicUtil::store(m_isWriting, false);
            return;
        }

        // 构建 const_buffer 序列，复用 m_writeBuffer 中的字符串数据
        m_sendBuffers.clear();
        for (size_t i = 0; i < count; ++i) {
            m_sendBuffers.emplace_back(boost::asio::buffer(m_writeBuffer[i]));
        }

        auto self = shared_from_this();
        auto onComplete = [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                self->reportError("发送数据失败: " + ec.message());
                self->close();
            } else {
                self->doWrite(); // 继续发送下一批消息
            }
        };

        // 使用串行化执行器确保发送操作按顺序执行
        boost::asio::async_write(m_socket,
                                 m_sendBuffers,
                                 boost::asio::bind_executor(m_writeStrand, onComplete));
    } catch (const std::exception& ex) {
        reportError("发送数据失败: " + std::string(ex.what()));
        close();
    } catch (...) {
        reportError("发送数据发生未知错误");
        close();
    }
}

void CTcpSession::reportError(const std::string& msg) noexcept
{
    if (m_errorcb) {
        m_errorcb(msg); // 直接调用错误回调，传递错误信息
    }
}

void CTcpSession::reportMessage(const char* data, size_t length) noexcept
{
    if (m_msgcb) {
        auto msg = std::string_view(data, length);
        m_msgcb(m_clientAddr, msg); // 直接调用消息回调，传递客户端地址和消息内容
    }
}

void CTcpSession::reportDisconnect() noexcept
{
    if (m_disconnectcb) {
        m_disconnectcb(m_clientAddr); // 直接调用断开回调，传递客户端地址
    }
}

} // namespace asio