#pragma once

#include "tcp/CTcpClient.h"

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <concurrentqueue-1.0.4/concurrentqueue.h>
#include "utils/AsioThreadPool.h"

namespace {

constexpr auto MAX_READ_LENGTH = 4096;

}

namespace asio {

/**
 * @brief 客户端连接生命周期状态枚举（State 模式）。
 *
 * @details 用单一原子枚举替代原先的 bool 组合，使状态转换在源码中可读且可强制校验。
 *
 * 合法转换路径：
 * @code
 *   Disconnected ──doConnect()──> Connecting ──onConnect() ok──> Connected
 *        ^                                                          |
 *        └─────────────── doDisconnect() / 连接失败 ─────────────────┘
 * @endcode
 */
enum class ClientState : uint8_t {
    Disconnected = 0, ///< 未连接，可发起 doConnect()
    Connecting = 1,   ///< 异步连接进行中，禁止重复连接
    Connected = 2     ///< 连接就绪，可收发数据
};

/**
 * @brief CTcpClient 的内部实现类（Pimpl 惯用法）。
 *
 * @details 将网络 I/O、重连策略、发送队列等细节完全隐藏在此类中，
 *          对外只通过 CTcpClient 公共接口暴露行为。
 *
 * 设计要点：
 * - IO 线程与 Task 线程分离，用户回调不占用 IO 线程。
 * - Strand 串行化所有 socket 操作，无需显式加锁。
 * - 指数退避自动重连，最大延迟 30s。
 * - 无锁发送队列（moodycamel::ConcurrentQueue）+ 批量 scatter-gather 写。
 *
 * @see CTcpClient
 */
class CTcpClient::Impl : public std::enable_shared_from_this<CTcpClient::Impl>
{
    ///< Socket 相关操作执行器，保证连接/读写状态机串行推进
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Threads = ThreadGroup;

public:
    /**
     * @brief 构造并初始化基础执行器对象。
     * @note 仅完成对象级初始化，不启动线程，不建立连接。
     *       构造完成后需先调用 init()，再调用 connect()。
     */
    Impl();

    /**
     * @brief 析构时确保资源回收。
     * @details 关闭 socket、停止 io_context 并 join 所有线程。
     * @note 幂等：若未初始化则直接返回。
     */
    ~Impl();

public:
    /**
     * @brief 初始化客户端运行时资源（State: 未初始化 → 已初始化）。
     * @return 成功返回 true；若已初始化或资源分配失败则返回 false。
     * @note 失败时 RAII Guard 保证 m_initialized 回滚为 false。
     */
    bool init() noexcept;

    /**
     * @brief 发起到目标地址的异步连接流程。
     * @param[in] serverAddr 服务端地址。
     * @return 连接任务投递成功返回 true；未初始化返回 false。
     * @note 调用前会先执行 disconnect() 确保旧连接已清理。
     * @warning 必须先调用 init() 成功后才可调用 connect()。
     */
    bool connect(const NetAddr& serverAddr) noexcept;

    /**
     * @brief 主动断开连接并取消自动重连。
     * @note 幂等：未初始化或已断开时静默返回。
     */
    void disconnect() noexcept;

    /**
     * @brief 将消息入队并触发异步写循环。
     * @param[in] data 待发送数据视图。
     * @return 成功入队返回 true；未初始化、未连接或数据为空返回 false。
     */
    bool send(std::string_view data) noexcept;

    /**
     * @brief 设置应用层事件回调（拷贝语义）。
     * @param[in] callback 回调结构体。
     * @note 投递到任务线程执行，线程安全。
     */
    void setCallback(const ClientCallback& callback) noexcept;

    /**
     * @brief 设置应用层事件回调（移动语义）。
     * @param[in] callback 右值引用，调用后原对象处于有效但未指定状态。
     */
    void setCallback(ClientCallback&& callback) noexcept;

private:
    /// @brief 在 IO strand 中发起异步连接。
    void doConnect() noexcept;

    /// @brief async_connect 完成回调，成功则启动读循环，失败则触发重连。
    void onConnect(const boost::system::error_code& ec) noexcept;

    /**
     * @brief 断开 socket 并按需触发重连定时器。
     * @param[in] isReTry 为 true 时启动指数退避重连；主动断开时传 false。
     */
    void doDisconnect(bool isReTry = true) noexcept;

    /// @brief 在 IO strand 中投递下一轮 async_read_some。
    void doRead() noexcept;

    /// @brief async_read_some 完成回调。
    void onRead(const boost::system::error_code& ec, size_t len) noexcept;

    /// @brief 批量出队并发起 async_write（scatter-gather）。
    void doWrite() noexcept;

    /// @brief async_write 完成回调，继续写循环或清除飞行标志。
    void onWrite(const boost::system::error_code& ec, size_t len) noexcept;

private:
    /**
     * @name 事件上报（Command 模式）
     * @brief 所有用户回调均通过 postTask 投递到任务线程执行，不占用 IO 线程。
     * @{
     */
    void reportConnected() noexcept;
    void reportDisconnected() noexcept;
    void reportMessageReceived(const char* data, size_t length) noexcept;
    void reportError(std::string msg) noexcept;
    /** @} */

    /**
     * @brief 向 IO strand 投递任务（在 IO 线程串行执行）。
     * @details 以弱指针捕获 self，回调执行时 lock() 失败（Impl 已销毁）则跳过。
     * @param[in] task 签名为 void(Impl&) 的可调用对象。
     */
    void postIo(std::function<void(Impl&)> task) noexcept;

    /**
     * @brief 向 Task strand 投递任务（在任务线程串行执行）。
     * @details 以弱指针捕获 self，回调执行时 lock() 失败（Impl 已销毁）则跳过。
     * @param[in] task 签名为 void(Impl&) 的可调用对象。
     */
    void postTask(std::function<void(Impl&)> task) noexcept;

    /**
     * @brief Template Method：线程池统一启动模板。
     *
     * @details 封装 "创建 work_guard → reserve → 批量 emplace 线程" 固定序列，
     *          init() 通过参数区分 IO 池和 Task 池，无重复代码。
     *
     * @param[in]  threadCount 线程数量。
     * @param[in]  context     目标 io_context 引用。
     * @param[out] workGuard   输出 work_guard，防止 context 自然退出。
     * @param[out] threads     输出已启动的线程集合。
     * @return 启动成功返回 true；任意步骤失败则调用 reportError() 并返回 false。
     */
    bool initializeContext(size_t threadCount,
                           boost::asio::io_context& context,
                           WorkGuardPtr& workGuard,
                           Threads& threads) noexcept;

private:
    std::atomic<bool> m_initialized{false}; ///< 初始化标志，确保 init() 只能成功调用一次

    /// @name IO 线程池——驱动 asio 网络事件
    /// @{
    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_reconnectTimer; ///< 指数退避重连定时器
    WorkGuardPtr m_ioWorkGuard;
    Threads m_ioThreads;
    Strand m_ioStrand; ///< 串行化 connect/read/write/disconnect
    std::array<char, MAX_READ_LENGTH> m_readBuffer;
    moodycamel::ConcurrentQueue<std::string> m_sendQueue; ///< 无锁发送队列
    std::vector<std::string> m_batchMessages;             ///< 批次数据存储，持有生命周期
    std::vector<boost::asio::const_buffer> m_batchViews;  ///< scatter-gather 视图
    /// @}

    /// @name Task 线程池——执行用户回调，与 IO 完全解耦
    /// @{
    boost::asio::io_context m_taskContext;
    Strand m_taskStrand;
    WorkGuardPtr m_taskWorkGuard;
    Threads m_taskThreads;
    /// @}

    std::atomic<ClientState> m_connectionState{
        ClientState::Disconnected};       ///< 连接状态（原子 CAS 保证线程安全转换）
    std::atomic<bool> m_isWriting{false}; ///< 写循环飞行标志，防止 async_write 期间缓冲区被覆写

    NetAddr m_connectTarget;      ///< 当前连接目标地址（仅在 IO strand 中读写）
    size_t m_reconnectAttempt{0}; ///< 连续重连次数，用于指数退避（仅在 IO strand 中读写）

    ClientCallback m_callback; ///< 应用层回调集合，仅在任务线程读写
};

} // namespace asio