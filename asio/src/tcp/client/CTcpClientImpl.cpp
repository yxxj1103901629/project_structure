#include "CTcpClientImpl.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

#include <algorithm>

using namespace boost::asio::ip;
using namespace boost::system;
using namespace boost::asio::error;
using namespace std::chrono_literals;

namespace {

// ─── 线程池常量 ───────────────────────────────────────────────────────────
constexpr size_t WRITE_BUFFER_BATCH = 16;
constexpr size_t IO_THREAD_COUNT = 1;
constexpr size_t WORKER_THREAD_COUNT = 2;

// ─── 指数退避常量 ───────────────────────────────────────────────────────────
constexpr auto RECONNECT_BASE_DELAY = 1s;
constexpr auto RECONNECT_MAX_DELAY = 30s;
constexpr size_t RECONNECT_MAX_EXPONENT = 5;

/**
 * @brief 计算指数退避重连延迟。
 * @details 延迟公式：min(base × 2^attempt, maxDelay)，每次调用后 attempt 自增。
 * @param[in,out] attempt 当前重连尝试次数，函数内部自动递增。
 * @return 返回本次应等待的时长。
 */
inline std::chrono::steady_clock::duration calculateReconnectDelay(size_t& attempt) noexcept
{
    const auto exp = std::min(attempt, RECONNECT_MAX_EXPONENT);
    auto delay = RECONNECT_BASE_DELAY * static_cast<int>(1ULL << exp);
    if (delay > RECONNECT_MAX_DELAY)
        delay = RECONNECT_MAX_DELAY;
    ++attempt;
    return delay;
}

} // namespace

namespace asio {

CTcpClient::Impl::Impl()
    : m_ioContext()
    , m_socket(m_ioContext)
    , m_reconnectTimer(m_ioContext)
    , m_ioStrand(boost::asio::make_strand(m_ioContext))
    , m_taskContext()
    , m_taskStrand(boost::asio::make_strand(m_taskContext))
{}

/* ─────────────────────────────────────────────────────────────────────────
** 析构：确保资源按顺序完整回收（顺序不可乱）
**   1. exchange(false) 幂等守卫，重复析构静默返回
**   2. 取消定时器 + 关闭 socket，终止所有飞行中的异步操作
**   3. 释放 work_guard + stop context，让线程从 run() 返回
**   4. join 所有线程，保证回调执行完毕
** ───────────────────────────────────────────────────────────────────────── */
CTcpClient::Impl::~Impl()
{
    // ① 幂等守卫：仅已初始化的实例才执行清理
    if (!CAtomicUtil::exchange(m_initialized, false))
        return;

    // ② 关闭 socket 和定时器，取消所有未完成的异步操作
    m_reconnectTimer.cancel();
    error_code ec;
    [[maybe_unused]] auto s = m_socket.shutdown(tcp::socket::shutdown_both, ec);
    [[maybe_unused]] auto c = m_socket.close(ec);

    // ③ 释放 work_guard + stop context，驱使线程从 run() 返回
    m_ioWorkGuard.reset();
    m_taskWorkGuard.reset();
    m_ioContext.stop();
    m_taskContext.stop();

    // ④ join 全部线程，此时所有已投递的回调均已执行完毕
    auto joinAll = [](Threads& threads) {
        for (auto& t : threads)
            if (t.joinable())
                t.join();
        threads.clear();
    };
    joinAll(m_ioThreads);
    joinAll(m_taskThreads);
}

/* ─────────────────────────────────────────────────────────────────────────
** init() — 初始化运行时资源
**
** 关键流程：
**   1. 原子检查防止重复 init
**   2. RAII Guard 保证失败时 m_initialized 回滚为 false
**   3. 预分配批量发送缓冲（避免运行时频繁分配）
**   4. 依次启动 IO 和 Task 线程池（Template Method）
** ───────────────────────────────────────────────────────────────────────── */
bool CTcpClient::Impl::init() noexcept
{
    // ① 防止重复 init
    if (CAtomicUtil::load(m_initialized))
        return false;

    // ② RAII Guard：失败时回滚 m_initialized = false
    struct Guard
    {
        CTcpClient::Impl* impl;
        bool ok = false;
        ~Guard() { CAtomicUtil::store(impl->m_initialized, ok); }
        void release() noexcept { ok = true; }
    } guard{this};

    // ③ 预分配批量发送缓冲，避免运行时频繁分配
    try {
        m_batchMessages.resize(WRITE_BUFFER_BATCH);
        m_batchViews.reserve(WRITE_BUFFER_BATCH);
    } catch (const std::exception& ex) {
        reportError("初始化缓冲失败: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("初始化缓冲发生未知异常");
        return false;
    }

    // ④ 依次启动 IO 池和 Task 池（Template Method）
    if (!initializeContext(IO_THREAD_COUNT, m_ioContext, m_ioWorkGuard, m_ioThreads))
        return false;
    if (!initializeContext(WORKER_THREAD_COUNT, m_taskContext, m_taskWorkGuard, m_taskThreads))
        return false;

    guard.release();
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
** connect() — 发起异步连接
**
** 先 disconnect() 清理旧连接，再向 IO strand 投递 doConnect() 任务。
** m_connectTarget 在 strand 内赋值，保证与 doConnect 的读操作无竞态。
** ───────────────────────────────────────────────────────────────────────── */
bool CTcpClient::Impl::connect(const NetAddr& serverAddr) noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("客户端未初始化");
        return false;
    }

    disconnect(); // 确保之前的连接已清理

    postIo([serverAddr](Impl& self) {
        self.m_connectTarget = serverAddr;
        self.doConnect();
    });

    return true;
}

/* disconnect() — 主动断开：取消重连定时器 + 传 isReTry=false 禁止自动重连。*/
void CTcpClient::Impl::disconnect() noexcept
{
    if (!CAtomicUtil::load(m_initialized))
        return;

    postIo([](Impl& self) {
        self.m_reconnectAttempt = 0;    // 重置退避计数，下次 connect() 从初始延迟开始
        self.m_reconnectTimer.cancel(); // 取消已挂起的重连定时器
        self.doDisconnect(false);       // false = 不自动重连
    });
}

/* send() — 无锁入队 + 条件触发写循环
** m_isWriting 飞行标志保证同一时刻只有一个 async_write 在进行，
** 写循环在 onWrite 中持续自驱，直到队列耗尽才清除标志。*/
bool CTcpClient::Impl::send(std::string_view data) noexcept
{
    if (data.empty())
        return false;

    if (!CAtomicUtil::load(m_initialized)) {
        reportError("客户端未初始化");
        return false;
    }

    if (CAtomicUtil::load(m_connectionState) != ClientState::Connected)
        return false;

    m_sendQueue.enqueue(std::string(data));

    // 仅当无飞行中的 async_write 时才触发，避免覆写正在使用的缓冲区
    postIo([](Impl& self) {
        if (!CAtomicUtil::exchange(self.m_isWriting, true))
            self.doWrite();
    });

    return true;
}

void CTcpClient::Impl::setCallback(const ClientCallback& callback) noexcept
{
    // 直接拷贝回调对象，确保调用者的回调对象生命周期与客户端一致
    postTask([callback](Impl& self) { self.m_callback = callback; });
}

void CTcpClient::Impl::setCallback(ClientCallback&& callback) noexcept
{
    // 移动回调对象，避免不必要的拷贝开销，适用于临时对象或不再使用的回调对象
    postTask([cb = std::move(callback)](Impl& self) { self.m_callback = std::move(cb); });
}

/* ─────────────────────────────────────────────────────────────────────────
** doConnect() — 状态转换：Disconnected → Connecting
**
** RAII Guard：store(Connecting) 后若构造 socket 或 async_connect 投递失败，
** Guard 析构时自动回退至 Disconnected，避免状态卡在 Connecting。
** ───────────────────────────────────────────────────────────────────────── */
void CTcpClient::Impl::doConnect() noexcept
{
    // ① 只有 Disconnected 状态才允许发起连接，防止重复连接
    if (CAtomicUtil::load(m_connectionState) != ClientState::Disconnected)
        return;

    // ② Disconnected → Connecting；失败时 Guard 回退
    CAtomicUtil::store(m_connectionState, ClientState::Connecting);
    struct Guard
    {
        CTcpClient::Impl* impl;
        bool ok = false;
        ~Guard()
        {
            if (!ok)
                CAtomicUtil::store(impl->m_connectionState, ClientState::Disconnected);
        }
        void release() noexcept { ok = true; }
    } guard{this};

    try {
        // ③ 解析目标地址
        error_code ec;
        auto addr = make_address(m_connectTarget.ip(), ec);
        if (ec) {
            reportError("地址解析失败: " + ec.message());
            return;
        }
        tcp::endpoint endpoint(addr, m_connectTarget.port());

        // ④ 重建 socket，确保之前的连接资源已释放
        m_socket = tcp::socket(m_ioContext);

        // ⑤ 发起异步连接，结果在 onConnect 中处理
        auto token = bind_executor(m_ioStrand, [self = shared_from_this()](error_code ec) {
            self->onConnect(ec);
        });
        m_socket.async_connect(endpoint, token);

        guard.release(); // 投递成功，状态由 onConnect 负责后续更新
    } catch (const std::exception& ex) {
        reportError("连接异常: " + std::string(ex.what()));
    } catch (...) {
        reportError("连接发生未知异常");
    }
}

/* onConnect() — 状态转换：Connecting → Connected 或 → Disconnected（含重连）
** 注意：失败时不能先 store(Disconnected)，否则 doDisconnect() 内的 exchange 判断
** 提前返回，导致重连定时器无法启动。*/
void CTcpClient::Impl::onConnect(const error_code& ec) noexcept
{
    if (ec) {
        reportError("连接失败: " + ec.message());
        doDisconnect(); // 内部 exchange(Disconnected) + 触发重连
        return;
    }

    // Connecting → Connected
    CAtomicUtil::store(m_connectionState, ClientState::Connected);
    m_reconnectAttempt = 0; // 连接成功，重置退避计数
    reportConnected();
    doRead(); // 启动异步读循环
}

/* ─────────────────────────────────────────────────────────────────────────
** doDisconnect() — 状态转换：任意 → Disconnected
**
** exchange 原子操作保证幂等：已经是 Disconnected 则直接返回。
** isReTry=true 时启动指数退避定时器自动重连；
** isReTry=false 时（主动断开）不重连。
** ───────────────────────────────────────────────────────────────────────── */
void CTcpClient::Impl::doDisconnect(bool isReTry) noexcept
{
    // ① 原子 exchange 保证幂等，已 Disconnected 则静默返回
    if (CAtomicUtil::exchange(m_connectionState, ClientState::Disconnected)
        == ClientState::Disconnected)
        return;

    // ② 重置写飞行标志，避免断线后无法触发新的写循环
    CAtomicUtil::store(m_isWriting, false);

    // ③ 关闭 socket，取消所有未完成的异步读写
    error_code ec;
    [[maybe_unused]] auto s = m_socket.shutdown(tcp::socket::shutdown_both, ec);
    [[maybe_unused]] auto c = m_socket.close(ec);

    reportDisconnected();

    // ④ 按需触发指数退避重连（1s、2s、4s…最大 30s）
    if (!isReTry)
        return;

    m_reconnectTimer.expires_after(calculateReconnectDelay(m_reconnectAttempt));
    auto self = shared_from_this();
    m_reconnectTimer.async_wait(bind_executor(m_ioStrand, [self](const error_code& ec) {
        if (ec == operation_aborted)
            return; // 定时器被取消（手动 disconnect() 触发）
        if (ec) {
            self->reportError("重连定时器异常: " + ec.message());
            return;
        }
        self->doConnect();
    }));
}

/* doRead() — 投递 async_read_some，非 Connected 状态时终止读循环。*/
void CTcpClient::Impl::doRead() noexcept
{
    if (CAtomicUtil::load(m_connectionState) != ClientState::Connected)
        return;

    auto self = shared_from_this();
    m_socket.async_read_some(boost::asio::buffer(m_readBuffer),
                             bind_executor(m_ioStrand, [self](error_code ec, size_t len) {
                                 self->onRead(ec, len);
                             }));
}

void CTcpClient::Impl::onRead(const error_code& ec, size_t len) noexcept
{
    if (ec) {
        if (ec == operation_aborted) {
            return; // socket 被主动关闭，正常退出读循环
        }
        // eof、connection_reset、connection_aborted 均表示对端关闭连接，触发重连
        reportError("读取数据失败: " + ec.message());
        doDisconnect();
        return;
    }

    if (len == 0) {
        // 读到0字节通常表示对端关闭了连接，主动断开连接以触发重连机制
        reportError("服务器关闭了连接");
        doDisconnect();
        return;
    }

    reportMessageReceived(m_readBuffer.data(), len);
    doRead(); // 继续读循环，保持持续读取服务器数据
}

/* doWrite() — 批量出队并发起 scatter-gather async_write。
** 队列为空时清除飞行标志，写循环自然终止；
** 非 Connected 状态时也清除标志，避免下次连接无法触发写循环。*/
void CTcpClient::Impl::doWrite() noexcept
{
    if (CAtomicUtil::load(m_connectionState) != ClientState::Connected) {
        CAtomicUtil::store(m_isWriting, false);
        return;
    }

    auto count = m_sendQueue.try_dequeue_bulk(m_batchMessages.data(), WRITE_BUFFER_BATCH);
    if (count == 0) {
        CAtomicUtil::store(m_isWriting, false);
        return;
    }

    m_batchViews.clear();
    for (size_t i = 0; i < count; ++i) {
        m_batchViews.emplace_back(boost::asio::buffer(m_batchMessages[i]));
    }

    // async_write 的 error_code 重载不抛异常，m_isWriting 保持 true 直到 onWrite 回调
    auto self = shared_from_this();
    boost::asio::async_write(m_socket,
                             m_batchViews,
                             bind_executor(m_ioStrand, [self](error_code ec, size_t len) {
                                 self->onWrite(ec, len);
                             }));
}

void CTcpClient::Impl::onWrite(const error_code& ec, size_t len) noexcept
{
    if (ec) {
        CAtomicUtil::store(m_isWriting, false); // 异步回调中手动重置
        reportError("发送数据失败: " + ec.message());
        doDisconnect();
        return;
    }

    // 继续写循环，检查是否有更多数据需要发送
    doWrite();
}

/* reportXxx — 事件上报：通过 postTask 投递到任务线程，不占用 IO 线程。*/
void CTcpClient::Impl::reportConnected() noexcept
{
    postTask([](Impl& self) {
        if (self.m_callback.connected)
            self.m_callback.connected(self.m_connectTarget);
    });
}

void CTcpClient::Impl::reportDisconnected() noexcept
{
    postTask([](Impl& self) {
        if (self.m_callback.disconnected)
            self.m_callback.disconnected(self.m_connectTarget);
    });
}

void CTcpClient::Impl::reportMessageReceived(const char* data, size_t length) noexcept
{
    auto msg = std::string(data, length);
    postTask([msg = std::move(msg)](Impl& self) {
        if (self.m_callback.messageReceived)
            self.m_callback.messageReceived(std::string_view(msg));
    });
}

void CTcpClient::Impl::reportError(const std::string& msg) noexcept
{
    postTask([msg](Impl& self) {
        if (self.m_callback.errorOccurred)
            self.m_callback.errorOccurred(msg);
    });
}

void CTcpClient::Impl::postIo(std::function<void(Impl&)> task) noexcept
{
    boost::asio::post(m_ioStrand, [self = shared_from_this(), task = std::move(task)]() mutable {
        task(*self);
    });
}

void CTcpClient::Impl::postTask(std::function<void(Impl&)> task) noexcept
{
    boost::asio::post(m_taskStrand, [self = shared_from_this(), task = std::move(task)]() mutable {
        task(*self);
    });
}

/* ─────────────────────────────────────────────────────────────────────────
** initializeContext() — Template Method：线程池统一启动模板
**
** 步骤固定：创建 work_guard → reserve 容量 → 批量 emplace 线程。
** 与 server 的 startPool 逻辑对齐，消除重复的 tempThreads/threadFailed 逻辑。
** ───────────────────────────────────────────────────────────────────────── */
bool CTcpClient::Impl::initializeContext(size_t threadCount,
                                         boost::asio::io_context& context,
                                         WorkGuardPtr& workGuard,
                                         Threads& threads) noexcept
{
    // ① work_guard 必须在线程启动前持有，否则 context.run() 可能立即返回
    workGuard = make_unique_noexcept<WorkGuard>(boost::asio::make_work_guard(context));
    if (!workGuard) {
        reportError("创建工作保护失败");
        return false;
    }

    // ② 批量创建工作线程；捕获 &context 保证引用同一 context
    try {
        threads.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++i) {
            threads.emplace_back([this, &context] {
                try {
                    context.run(); // 阻塞，直到 work_guard 释放且任务耗尽
                } catch (const std::exception& ex) {
                    reportError("线程异常: " + std::string(ex.what()));
                } catch (...) {
                    reportError("线程发生未知异常");
                }
            });
        }
    } catch (const std::exception& ex) {
        reportError("启动线程失败: " + std::string(ex.what()));
        return false;
    }

    return true;
}

} // namespace asio