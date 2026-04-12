#include "CTcpServerImpl.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

using namespace boost::asio;
using namespace boost::system;
using namespace boost::asio::error;

namespace {

/* 推算默认线程总数：IO 密集型场景取硬件并发数的 2 倍，最少保证 2 条线程，
** 避免单核环境下 IO 线程与 Task 线程全部塌缩为 0。*/
size_t getHardwareConcurrency() noexcept
{
    return std::max(2u, std::thread::hardware_concurrency() * 2u);
}

} // namespace

namespace asio {

/* ─────────────────────────────────────────────────────────────────────────
** 构造 / 析构
** ───────────────────────────────────────────────────────────────────────── */

CTcpServer::Impl::Impl()
    : m_ioContext()
    , m_acceptor(m_ioContext)
{}

/* 析构时调用 stop() 确保线程安全退出；stop() 内部有状态守卫，重复调用无害。*/
CTcpServer::Impl::~Impl()
{
    stop();
}

/* ─────────────────────────────────────────────────────────────────────────
** init() — 状态转换：Uninitialized → Idle
**
** 只计算线程分配比例，不启动任何线程；线程在 listen() 中才真正创建。
** IO 线程占约 1/3，任务线程占约 2/3；比例来自典型网络服务器经验值。
** ───────────────────────────────────────────────────────────────────────── */

bool CTcpServer::Impl::init(size_t threads) noexcept
{
    // ① 原子 CAS 确保只有 Uninitialized 状态可以进入，防止重复 init
    ServerState expected = ServerState::Uninitialized;
    if (!CAtomicUtil::compareExchange(m_state, expected, ServerState::Idle))
        return false;

    // ② 0 表示自动推导
    if (threads == 0)
        threads = getHardwareConcurrency();

    // ③ IO 线程不宜过多，否则 context 竞争反而拖慢吞吐；任务线程承载用户回调
    m_ioThreadCount = std::max<size_t>(1, threads / 3);
    m_taskThreadCount = std::max<size_t>(1, threads - m_ioThreadCount);
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
** listen() — 状态转换：Idle → Running
**
** 关键流程：
**   1. acceptor 配置（open / reuse_addr / bind / listen）—— 短路求值，任一失败即终止
**   2. 启动 IO 线程池（驱动 asio 事件循环）
**   3. 启动 Task 线程池（执行用户回调）
**   4. 投递首个 async_accept，开启接入循环
**
** RAII Guard：若步骤 2/3 失败，Guard 析构时自动调用 stop() 回滚状态，
** 避免服务器处于"Running 但线程池为空"的半初始化状态。
** ───────────────────────────────────────────────────────────────────────── */

bool CTcpServer::Impl::listen(uint16_t port) noexcept
{
    // ① 原子 CAS：Idle → Running，非 Idle 状态（含未 init 或已 Running）直接拒绝
    ServerState expected = ServerState::Idle;
    if (!CAtomicUtil::compareExchange(m_state, expected, ServerState::Running))
        return false;

    // ② RAII 回滚守卫：提前进入 Running 状态，一旦后续步骤失败则自动 stop()
    struct Guard
    {
        Impl* self;
        bool ok{false};
        ~Guard()
        {
            if (!ok)
                self->stop();
        }
        void release() { ok = true; }
    } guard{this};

    // ③ acceptor 配置：四步短路——任意一步出错立即跳出并上报
    error_code ec;
    tcp::endpoint ep(tcp::v4(), port);
    if (m_acceptor.open(ep.protocol(), ec)                             //
        || m_acceptor.set_option(socket_base::reuse_address(true), ec) //
        || m_acceptor.bind(ep, ec)                                     //
        || m_acceptor.listen(socket_base::max_listen_connections, ec)  //
    ) {
        notifyError("配置acceptor失败: " + ec.message());
        return false;
    }

    // ④ 依次启动 IO 池和 Task 池（Template Method）
    if (!startPool(m_ioThreadCount, m_ioContext, m_ioWorkGuard, m_ioThreads)            //
        || !startPool(m_taskThreadCount, m_taskContext, m_taskWorkGuard, m_taskThreads) //
    )
        return false;

    // ⑤ 全部就绪才解除 Guard，开始接受连接
    guard.release();
    onAccept();
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
** startPool() — Template Method：线程池统一启动模板
**
** 步骤固定：创建 work_guard → reserve 容量 → 批量 emplace 线程。
** work_guard 防止 context 在所有任务完成前自动退出；
** 每条线程内部捕获 std::exception 和 ... 两层，确保异常不逃逸到 std::terminate。
** ───────────────────────────────────────────────────────────────────────── */

bool CTcpServer::Impl::startPool(size_t n,
                                 boost::asio::io_context& ctx,
                                 WorkGuardPtr& wg,
                                 Threads& pool) noexcept
{
    // ① work_guard 必须在线程启动前持有，否则 context.run() 可能立即返回
    wg = make_unique_noexcept<WorkGuard>(make_work_guard(ctx));
    if (!wg) {
        notifyError("work guard alloc failed");
        return false;
    }

    // ② 批量创建工作线程；捕获 &ctx 而非 this 拷贝，保证引用同一 context
    try {
        pool.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            pool.emplace_back([this, &ctx] {
                try {
                    ctx.run(); // 阻塞，直到 work_guard 释放且任务耗尽
                } catch (const std::exception& e) {
                    notifyError(e.what());
                } catch (...) {
                    notifyError("unknown thread exception");
                }
            });
        }
    } catch (const std::exception& e) {
        notifyError(e.what());
        return false;
    }
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
** stop() — 状态转换：Running → Idle
**
** 关键流程（顺序不可乱）：
**   1. acceptor 取消 + 关闭，触发 onAccept 以 operation_aborted 结束循环，释放 shared_ptr<Impl>
**   2. O(1) 原子接管 session 映射
**   3. 关闭所有 session（异步 shutdown+close 投递到 writeStrand）
**   4. reset IO work_guard，让 IO 自然排空（所有 close/read 回调结束后 run() 返回）
**   5. join IO 线程（等待所有 IO 回调彻底完成，不遗留任何 shared_ptr 引用）
**   6. reset task work_guard + join task 线程（drain 所有已投递的用户回调）
**   7. 直接调用 clientDisconnected（针对 stop() 主动关闭的 session）
**   8. restart context，允许后续重新 listen
**
** !! 不调用 ioContext.stop() !!
**    stop() 会遗弃仍持有 shared_ptr<Impl> 的 pending handler（如 onAccept lambda），
**    导致 Impl 引用计数永不归零，~Impl() 永远不会运行。
** ───────────────────────────────────────────────────────────────────────── */

void CTcpServer::Impl::stop() noexcept
{
    // ① 原子 CAS：仅 Running 状态可以停止，幂等保护
    ServerState expected = ServerState::Running;
    if (!CAtomicUtil::compareExchange(m_state, expected, ServerState::Idle))
        return;

    // ② 取消/关闭 acceptor；onAccept lambda 收到 operation_aborted 后检查状态退出循环，
    //   释放捕获的 shared_ptr<Impl>，Impl 引用计数恢复正常
    boost::system::error_code ec;
    [[maybe_unused]] auto r1 = m_acceptor.cancel(ec);
    [[maybe_unused]] auto r2 = m_acceptor.close(ec);

    // ③ O(1) 接管 session 映射（std::move 而非逐条 erase），最小化写锁持有时间
    SessionMap sessions;
    {
        std::unique_lock lock(m_sessionsMutex);
        sessions = std::move(m_sessions);
    }

    // ④ 对每个 session 投递异步 shutdown+close；socket 关闭后 async_read_some 以错误完成，
    //   doRead 退出，readStrand 上无更多工作
    for (auto& kv : sessions)
        kv.second->close();

    // ⑤ 只 reset work_guard，不调用 ioContext.stop()——让 IO context 自然排空后退出；
    //   所有 accept/close/read 回调处理完毕后 run() 自动返回
    m_ioWorkGuard.reset();
    auto joinAll = [](Threads& pool) {
        for (auto& t : pool)
            if (t.joinable())
                t.join();
        pool.clear();
    };
    joinAll(m_ioThreads); // 等待所有 IO 回调（close/read/accept）彻底完成

    // ⑥ drain task 线程（IO 线程退出后不再产生新任务，让已有回调全部执行完毕）
    m_taskWorkGuard.reset();
    joinAll(m_taskThreads);

    // ⑦ 直接在当前线程触发 clientDisconnected，针对 stop() 主动关闭的 session；
    //   自然断开的 session 已在 onSessionDisconnected→postTask 路径触发（⑥ drain 时执行）
    if (m_callback.clientDisconnected) {
        for (auto& kv : sessions)
            m_callback.clientDisconnected(kv.first);
    }

    // ⑧ restart 使 context 可被下一次 listen 重复使用
    m_ioContext.restart();
    m_taskContext.restart();
}

/* ─────────────────────────────────────────────────────────────────────────
** 数据收发 / 连接管理
** ───────────────────────────────────────────────────────────────────────── */

bool CTcpServer::Impl::send2c(const NetAddr& clientAddr, std::string_view data) noexcept
{
    auto session = getSession(clientAddr);
    if (!session)
        return false;
    session->send(std::string(data));
    return true;
}

bool CTcpServer::Impl::broadcast(std::string_view data) noexcept
{
    // 先在读锁下拷贝快照，广播期间不持锁，允许并发注册新 session
    std::vector<std::shared_ptr<CTcpSession>> snapshot;
    {
        std::shared_lock lock(m_sessionsMutex);
        snapshot.reserve(m_sessions.size());
        for (auto& kv : m_sessions)
            snapshot.push_back(kv.second);
    }
    for (auto& s : snapshot)
        s->send(std::string(data));
    return true;
}

void CTcpServer::Impl::disconnect(const NetAddr& clientAddr) noexcept
{
    auto session = getSession(clientAddr);
    if (session) {
        session->close();
        // 主动断开也需要走 onSessionDisconnected，保证映射清理和回调对称
        onSessionDisconnected(clientAddr);
    }
}

/* setCallback 投递到任务线程执行，保证 m_callback 只在单一线程写入，无需加锁。*/
void CTcpServer::Impl::setCallback(const ServerCallback& callback) noexcept
{
    postTask([callback](Impl& self) { self.m_callback = callback; });
}

void CTcpServer::Impl::setCallback(ServerCallback&& callback) noexcept
{
    postTask([cb = std::move(callback)](Impl& self) { self.m_callback = std::move(cb); });
}

/* ─────────────────────────────────────────────────────────────────────────
** onAccept() — 异步接入循环
**
** 每次 async_accept 完成后：
**   - 以弱指针捕获 self；lock() 失败（Impl 已销毁）或状态非 Running 时终止循环
**   - 出错则上报，但继续投递下一轮（瞬时错误不中断服务）
**   - 成功则将 socket 交给 registerSession 完成后续注册
** ───────────────────────────────────────────────────────────────────────── */

void CTcpServer::Impl::onAccept() noexcept
{
    auto weak = make_weak_noexcept(shared_from_this());
    m_acceptor.async_accept([weak](error_code ec, tcp::socket socket) {
        auto self = weak.lock();
        // 服务器已销毁或已停止，终止接入循环
        if (!self || CAtomicUtil::load(self->m_state) != ServerState::Running)
            return;

        if (ec)
            self->notifyError(ec.message()); // 瞬时错误：上报后继续监听
        else
            self->registerSession(std::move(socket));

        self->onAccept(); // 持续循环，准备接受下一个连接
    });
}

/* Factory Method：将 CTcpSession 构造细节封装在此，registerSession 无需关心。*/
std::shared_ptr<CTcpSession> CTcpServer::Impl::createSession(tcp::socket socket) noexcept
{
    return make_shared_noexcept<CTcpSession>(std::move(socket), m_ioContext, this);
}

/* ─────────────────────────────────────────────────────────────────────────
** registerSession() — 新连接完整注册流程
**
** 步骤：① 工厂创建 → ② 启动异步读 → ③ 写入映射 → ④ 触发 connected 回调
** ───────────────────────────────────────────────────────────────────────── */

void CTcpServer::Impl::registerSession(tcp::socket socket) noexcept
{
    // ① Factory Method 创建 session（失败通常是 OOM）
    auto session = createSession(std::move(socket));
    if (!session) {
        notifyError("构建session失败");
        return;
    }

    // ② 启动异步读循环；失败说明 socket 状态异常，直接丢弃
    if (!session->start()) {
        notifyError("启动session失败");
        return;
    }

    // ③ 写入 session 映射，使后续 send / disconnect / getSession 可检索到
    const NetAddr addr = session->getClientAddr();
    {
        std::unique_lock lock(m_sessionsMutex);
        m_sessions.emplace(addr, std::move(session));
    }

    // ④ 在任务线程回调，避免占用 IO 线程时间
    postTask([addr](Impl& self) {
        if (self.m_callback.clientConnected)
            self.m_callback.clientConnected(addr);
    });
}

std::shared_ptr<CTcpSession> CTcpServer::Impl::getSession(const NetAddr& addr) const noexcept
{
    std::shared_lock lock(m_sessionsMutex); // 读锁，允许并发查询
    auto it = m_sessions.find(addr);
    return it != m_sessions.end() ? it->second : nullptr;
}

/* ─────────────────────────────────────────────────────────────────────────
** 事件上报（ISessionObserver 实现 + 内部错误路由）
** ───────────────────────────────────────────────────────────────────────── */

/* 所有错误（来自 session 或 Impl 自身）统一经此入队，在任务线程触发用户回调。*/
void CTcpServer::Impl::notifyError(const std::string& msg) noexcept
{
    postTask([msg](Impl& self) {
        if (self.m_callback.errorOccurred)
            self.m_callback.errorOccurred(msg);
    });
}

/* 断开连接：先从映射中移除（若已不存在则防止重复回调），再触发 disconnected。*/
void CTcpServer::Impl::onSessionDisconnected(const NetAddr& addr) noexcept
{
    {
        std::unique_lock lock(m_sessionsMutex);
        if (m_sessions.erase(addr) == 0)
            return; // 已被 stop() 清理，避免重复触发回调
    }
    postTask([addr](Impl& self) {
        if (self.m_callback.clientDisconnected)
            self.m_callback.clientDisconnected(addr);
    });
}

/* 消息到达：buf 按值捕获，延长数据生命周期至任务线程执行完毕。*/
void CTcpServer::Impl::onSessionMessage(const NetAddr& addr, std::string_view msg) noexcept
{
    postTask([addr, buf = std::string(msg)](Impl& self) {
        if (self.m_callback.messageReceived)
            self.m_callback.messageReceived(addr, std::string_view(buf));
    });
}

/* session 错误直接转发至统一的 notifyError 路径。*/
void CTcpServer::Impl::onSessionError(const std::string& msg) noexcept
{
    notifyError(msg);
}

/* ─────────────────────────────────────────────────────────────────────────
** postTask() — Command 模式：任务投递
**
** 以弱指针捕获 self，回调执行时 lock() 失败（Impl 已销毁）则跳过，
** 不持有强引用，不延长 Impl 生命周期；
** task 以值语义移动进 lambda 避免拷贝开销。
** ───────────────────────────────────────────────────────────────────────── */

void CTcpServer::Impl::postTask(std::function<void(Impl&)> task) noexcept
{
    auto weak = make_weak_noexcept(shared_from_this());
    post(m_taskContext, [weak, t = std::move(task)]() mutable {
        if (auto self = weak.lock())
            t(*self);
    });
}

} // namespace asio
