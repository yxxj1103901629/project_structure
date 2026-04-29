#include "CTcpSession.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

using namespace boost::asio;
using namespace boost::system;
using namespace boost::asio::error;

namespace {

constexpr size_t WRITE_BUFFER_BATCH = 16;

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
    CAtomicUtil::store(m_isConnected, false);
    closeSocket();
}

bool CTcpSession::start() noexcept
{
    if (!CAtomicUtil::load(m_isConnected)) {
        return false;
    }

    try {
        const auto endpoint = m_socket.remote_endpoint();
        m_clientAddr = NetAddr{endpoint.address().to_string(), endpoint.port()};
        m_writeBuffer.resize(WRITE_BUFFER_BATCH);
        m_sendBuffers.reserve(WRITE_BUFFER_BATCH);
    } catch (const std::exception& ex) {
        reportError("failed to start session: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("failed to start session: unknown error");
        return false;
    }

    doRead();
    return true;
}

void CTcpSession::send(std::string_view data) noexcept
{
    if (data.empty() || !CAtomicUtil::load(m_isConnected)) {
        return;
    }

    m_msgQueue.enqueue(std::string(data));

    dispatch(m_writeStrand, [weak = make_weak_noexcept(shared_from_this())]() {
        if (auto self = weak.lock(); self && !CAtomicUtil::exchange(self->m_isWriting, true)) {
            self->doWrite();
        }
    });
}

void CTcpSession::send(std::string&& data) noexcept
{
    if (data.empty() || !CAtomicUtil::load(m_isConnected)) {
        return;
    }

    m_msgQueue.enqueue(std::move(data));

    dispatch(m_writeStrand, [weak = make_weak_noexcept(shared_from_this())]() {
        if (auto self = weak.lock(); self && !CAtomicUtil::exchange(self->m_isWriting, true)) {
            self->doWrite();
        }
    });
}

void CTcpSession::close() noexcept
{
    if (!CAtomicUtil::exchange(m_isConnected, false)) {
        return;
    }

    dispatch(m_writeStrand, [weak = make_weak_noexcept(shared_from_this())]() {
        if (auto self = weak.lock()) {
            self->closeSocket();
        }
    });
}

void CTcpSession::forceClose() noexcept
{
    CAtomicUtil::store(m_isConnected, false);
    closeSocket();
}

void CTcpSession::doRead() noexcept
{
    if (!CAtomicUtil::load(m_isConnected)) {
        return;
    }

    auto weak = make_weak_noexcept(shared_from_this());
    m_socket.async_read_some(buffer(m_readBuffer), bind_executor(m_readStrand, [weak](error_code ec, std::size_t len) {
                                 auto self = weak.lock();
                                 if (!self) {
                                     return;
                                 }

                                 if (!ec && len > 0) {
                                     self->reportData(self->m_readBuffer.data(), len);
                                     self->doRead();
                                     return;
                                 }

                                 if (ec != operation_aborted && ec != eof && ec != connection_reset
                                     && ec != connection_aborted) {
                                     self->reportError("failed to read data: " + ec.message());
                                 }

                                 self->finishDisconnect();
                             }));
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
        m_sendBuffers.emplace_back(buffer(m_writeBuffer[i]));
    }

    auto weak = make_weak_noexcept(shared_from_this());
    async_write(m_socket, m_sendBuffers, bind_executor(m_writeStrand, [weak](error_code ec, std::size_t) {
                    auto self = weak.lock();
                    if (!self) {
                        return;
                    }

                    if (ec) {
                        CAtomicUtil::store(self->m_isWriting, false);
                        if (ec != operation_aborted) {
                            self->reportError("failed to send data: " + ec.message());
                        }
                        self->finishDisconnect();
                        return;
                    }

                    self->doWrite();
                }));
}

void CTcpSession::closeSocket() noexcept
{
    error_code ec;
    [[maybe_unused]] const auto shutdownResult = m_socket.shutdown(tcp::socket::shutdown_both, ec);
    [[maybe_unused]] const auto closeResult = m_socket.close(ec);
}

void CTcpSession::finishDisconnect() noexcept
{
    const bool wasConnected = CAtomicUtil::exchange(m_isConnected, false);
    closeSocket();
    CAtomicUtil::store(m_isWriting, false);
    if (wasConnected) {
        reportDisconnect();
    } else if (!CAtomicUtil::load(m_disconnectReported)) {
        reportDisconnect();
    }
}

void CTcpSession::reportError(const std::string& msg) noexcept
{
    if (m_observer) {
        m_observer->onSessionError(msg);
    }
}

void CTcpSession::reportData(const char* data, size_t length) noexcept
{
    if (m_observer) {
        m_observer->onSessionData(m_clientAddr, {data, length});
    }
}

void CTcpSession::reportDisconnect() noexcept
{
    if (CAtomicUtil::exchange(m_disconnectReported, true)) {
        return;
    }

    if (m_observer) {
        m_observer->onSessionDisconnected(m_clientAddr);
    }
}

} // namespace asio
