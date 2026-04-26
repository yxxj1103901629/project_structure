#pragma once

#include "tcp/CTcpServer.h"
#include "CTcpSession.h"

#include <boost/asio.hpp>
#include <memory>
#include "utils/AsioThreadPool.h"

namespace asio {

using boost::asio::ip::tcp;

/**
 * @brief 服务器生命周期状态枚举（State 模式）。
 *
 * @details 用单一原子枚举替代原先分散的 m_initialized + m_isRunning 两个布尔标志，
 *          使状态转换在源码中可读且可强制校验。
 *
 * 合法转换路径：
 * @code
 *   Uninitialized ──init()──> Idle ──listen()──> Running ──stop()──> Idle
 *                                    ^─────────────────────────────────┘
 *                                         （可重复 listen）
 * @endcode
 */
enum class ServerState : uint8_t {
    Uninitialized, ///< 刚构造，init() 尚未调用
    Idle,          ///< init() 完成，或已 stop()，可重新 listen
    Running        ///< listen() 激活，正在接受连接
};

/**
 * @brief CTcpServer 的内部实现类（Pimpl 惯用法）。
 *
 * @details 将网络 I/O、线程池管理、会话生命周期等细节完全隐藏在此类中，
 *          对外只通过 CTcpServer 公共接口暴露行为。
 *
 * 设计要点：
 * - 继承 std::enable_shared_from_this，在异步回调中以弱指针捕获 self，
 *   回调执行时 lock() 失败即跳过，不延长 Impl 生命周期。
 * - 继承 ISessionObserver，直接接收每个 CTcpSession 上报的事件，
 *   无需为每个 session 单独注册独立的 lambda。
 * - IO 线程池与 Task 线程池分离，用户回调不占用 IO 线程。
 *
 * @see ISessionObserver
 * @see CTcpServer
 */
class CTcpServer::Impl : public std::enable_shared_from_this<CTcpServer::Impl>,
                         public ISessionObserver
{
    using Threads = ThreadGroup;
    using SessionMap = std::unordered_map<NetAddr, std::shared_ptr<CTcpSession>>;

public:
    Impl();
    ~Impl();

    /**
     * @brief 初始化线程池规模（State: Uninitialized → Idle）。
     * @param[in] threads 总线程数，传 0 时自动按硬件并发数 × 2 推导。
     * @return 成功返回 true；若状态不是 Uninitialized 则返回 false（禁止重复 init）。
     */
    bool init(size_t threads) noexcept;

    /**
     * @brief 绑定端口并开始异步接受连接（State: Idle → Running）。
     * @param[in] port 监听端口号（1–65535）。
     * @return 成功启动返回 true；若状态不是 Idle、或 acceptor/线程池配置失败则返回 false。
     * @note 失败时 RAII Guard 自动调用 stop() 将状态回滚至 Idle，
     *       保证服务器不会停留在半初始化的 Running 状态。
     * @warning 必须先调用 init() 成功后才可调用 listen()。
     */
    bool listen(uint16_t port) noexcept;

    /**
     * @brief 停止服务器，关闭所有连接并销毁线程池（State: Running → Idle）。
     * @note 幂等：若当前不是 Running 状态则静默返回，可安全重复调用。
     * @note 线程池 join 完成后 context 被 restart，允许后续重新调用 listen()。
     */
    void stop() noexcept;

    /**
     * @brief 向指定客户端发送数据。
     * @param[in] clientAddr 目标客户端地址。
     * @param[in] data       待发送数据视图（调用方需保证数据生命周期覆盖入队瞬间）。
     * @return 找到对应 session 并成功入队返回 true，session 不存在返回 false。
     */
    bool send2c(const NetAddr& clientAddr, std::string_view data) noexcept;

    /**
     * @brief 向当前所有已连接客户端广播数据。
     * @param[in] data 待广播数据视图。
     * @return 始终返回 true（广播不区分单个 session 是否成功）。
     * @note 先在读锁下拷贝 session 快照，广播期间不持锁，
     *       允许并发注册新 session 而不阻塞。
     */
    bool broadcast(std::string_view data) noexcept;

    /**
     * @brief 主动断开指定客户端连接。
     * @param[in] clientAddr 目标客户端地址。
     * @note 断开后会触发 clientDisconnected 回调，与对端主动断开行为对称。
     */
    void disconnect(const NetAddr& clientAddr) noexcept;

    /**
     * @brief 设置应用层事件回调（拷贝语义）。
     * @param[in] callback 回调结构体，内含 clientConnected / clientDisconnected /
     *                     messageReceived / errorOccurred 四个可选 std::function。
     * @note 投递到任务线程执行，线程安全，m_callback 仅在任务线程写入。
     */
    void setCallback(const ServerCallback& callback) noexcept;

    /**
     * @brief 设置应用层事件回调（移动语义，避免拷贝开销）。
     * @param[in] callback 右值引用，调用后原对象处于有效但未指定状态。
     */
    void setCallback(ServerCallback&& callback) noexcept;

private:
    /**
     * @brief Template Method：线程池统一启动模板。
     *
     * @details 封装"创建 work_guard → reserve 容量 → 批量 emplace 线程"三步固定序列，
     *          listen() 通过参数区分 IO 池和 Task 池，启动逻辑完全收敛在此，避免重复代码。
     *
     * @param[in]  n    线程数量。
     * @param[in]  ctx  目标 io_context 引用。
     * @param[out] wg   输出 work_guard，持有它可防止 context 在任务耗尽前自然退出。
     * @param[out] pool 输出已启动的线程集合。
     * @return 启动成功返回 @c true；任意步骤失败则调用 notifyError() 并返回 @c false。
     */
    bool startPool(size_t n, boost::asio::io_context& ctx, WorkGuardPtr& wg, Threads& pool) noexcept;

    /**
     * @brief Factory Method：CTcpSession 工厂。
     *
     * @details 将 session 构造与参数配置集中在此，registerSession() 只负责注册流程，
     *          职责边界清晰；派生 Impl 可重写此方法注入 TLS 或自定义 Session 子类。
     *
     * @param[in] socket 已 accept 的原始 socket（所有权转移）。
     * @return 成功返回 session 共享指针；内存分配失败返回 nullptr。
     */
    std::shared_ptr<CTcpSession> createSession(tcp::socket socket) noexcept;

    /**
     * @brief 投递下一轮 async_accept，循环驱动客户端接入。
     * @note 回调以弱指针捕获 self；lock() 失败或状态非 Running 时终止循环。
     */
    void onAccept() noexcept;

    /**
     * @brief 接受新 socket 后完成 session 的完整注册流程。
     *
     * @details 步骤：① createSession() 工厂创建 → ② start() 启动异步读
     *          → ③ 写入 session 映射 → ④ postTask 触发 clientConnected 回调。
     *
     * @param[in] socket 已 accept 的原始 socket（所有权转移）。
     */
    void registerSession(tcp::socket socket) noexcept;

    /**
     * @brief 在读锁保护下按地址查找 session。
     * @param[in] addr 客户端地址键。
     * @return 找到返回对应的 shared_ptr，不存在返回 @c nullptr。
     */
    std::shared_ptr<CTcpSession> getSession(const NetAddr& addr) const noexcept;

    /**
     * @brief 内部错误统一分发入口。
     *
     * @details Impl 自身（acceptor 配置、线程池启动等）产生的错误经此路由至
     *          用户 errorOccurred 回调，在语义上与 ISessionObserver::onSessionError 明确区分。
     *
     * @param[in] msg 错误描述字符串。
     */
    void notifyError(std::string msg) noexcept;

    /**
     * @name ISessionObserver 实现
     * @brief session 事件均通过 postTask 转入任务线程队列，
     *        确保用户回调不在 IO strand 中执行，避免阻塞 IO 线程。
     * @{
     */
    void onSessionError(const std::string& msg) noexcept override;
    void onSessionMessage(const NetAddr& addr, std::string_view msg) noexcept override;
    void onSessionDisconnected(const NetAddr& addr) noexcept override;
    /** @} */

    /**
     * @brief Command 模式：将任务投递到专用任务线程执行。
     *
     * @details 以弱指针捕获 self，回调执行时 lock() 失败（Impl 已销毁）则跳过，
     *          不阻止 Impl 析构；task 以移动语义传入，避免 std::function 的多余拷贝。
     *
     * @param[in] task 待执行的可调用对象，签名为 @c void(Impl&)。
     */
    void postTask(std::function<void(Impl&)> task) noexcept;

private:
    std::atomic<ServerState> m_state{
        ServerState::Uninitialized}; ///< 服务器生命周期状态（原子 CAS 保证线程安全转换）

    /// @name IO 线程池——专门驱动 asio 网络事件
    /// @{
    boost::asio::io_context m_ioContext;
    tcp::acceptor m_acceptor;
    WorkGuardPtr m_ioWorkGuard;
    Threads m_ioThreads;
    size_t m_ioThreadCount{0};
    /// @}

    /// @name Task 线程池——专门执行用户回调，与 IO 完全解耦
    /// @{
    boost::asio::io_context m_taskContext;
    WorkGuardPtr m_taskWorkGuard;
    Threads m_taskThreads;
    size_t m_taskThreadCount{0};
    /// @}

    SessionMap m_sessions;                     ///< 活跃 session 映射表，以 NetAddr 为键
    mutable std::shared_mutex m_sessionsMutex; ///< 保护 m_sessions 的共享读写锁

    ServerCallback m_callback; ///< 应用层注册的事件回调集合，仅在任务线程读写
};

} // namespace asio