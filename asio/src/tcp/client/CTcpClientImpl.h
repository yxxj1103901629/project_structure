#pragma once

#include "tcp/CTcpClient.h"
#include "tcp/client/CReconnectController.h"

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <concurrentqueue-1.0.4/concurrentqueue.h>

namespace {

constexpr auto MAX_READ_LENGTH = 4096;

}

namespace asio {

class CTcpClient::Impl : public std::enable_shared_from_this<CTcpClient::Impl>
{
public:
    /**
     * @brief 构造并初始化基础执行器对象。
     * @details 仅完成对象级初始化，不启动线程，不建立连接。
     * @note 构造完成后需先调用 init()，再调用 connect()。
     */
    Impl();

    /**
     * @brief 析构时确保资源回收。
     * @details 调用 stop() 关闭 socket、停止 io_context 并回收线程。
     */
    ~Impl();

public:
    /**
     * @brief 初始化客户端运行时资源。
     * @return 初始化成功返回 true，否则返回 false。
     */
    bool init() noexcept;

    /**
     * @brief 发起到目标地址的异步连接流程。
     * @param serverAddr 服务端地址。
     * @return 连接流程成功启动返回 true，否则返回 false。
     */
    bool connect(const NetAddr& serverAddr) noexcept;

    /**
     * @brief 主动断开连接并停止重连。
     */
    void disconnect() noexcept;

    /**
     * @brief 将消息加入发送队列并触发写循环。
     * @param data 待发送数据。
     * @return 入队成功返回 true，否则返回 false。
     */
    bool send(std::string_view data) noexcept;

    /**
     * @brief 设置客户端回调（拷贝）。
     */
    void setCallback(const ClientCallback& callback) noexcept;

    /**
     * @brief 设置客户端回调（移动）。
     */
    void setCallback(ClientCallback&& callback) noexcept;

private:
    using tcp = boost::asio::ip::tcp;
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkGuardPtr = std::unique_ptr<WorkGuard>;
    using Threads = std::vector<std::thread>;

    /**
     * @brief 客户端生命周期状态。
     */
    enum class LifecycleState : uint8_t {
        Created = 0,  // 对象已创建但未初始化
        Running = 1,  // 客户端已初始化并可正常使用
        Stopping = 2, // 客户端正在停止中，等待相关回调完成后进入 Stopped
        Stopped = 3,  // 客户端已完全停止，禁止一切操作
    };

    /**
     * @brief 连接状态机状态。
     */
    enum class ConnectionState : uint8_t {
        Idle = 0,          // 未连接，或连接已断开且不打算重连
        Connecting = 1,    // 连接中，尚未完成三次握手
        Connected = 2,     // 连接已完成，读写循环正常运行
        Reconnecting = 3,  // 连接已断开，正在等待重连时机或重连中
        Disconnecting = 4, // 连接正在断开中，等待相关回调完成后进入 Idle 或 Reconnecting
        Stopped = 5,       // 客户端已停止，禁止一切网络操作
    };

    /**
     * @brief 断链后的状态收敛模式。
     */
    enum class DisconnectMode : uint8_t {
        KeepIdle = 0,           // 断开后保持空闲状态
        ReconnectIfEnabled = 1, // 断开后如果启用重连则尝试重连
    };

    /**
     * @brief Socket 关闭执行模式。
     */
    enum class SocketCloseMode : uint8_t {
        Immediate = 0,     // 立即在当前线程关闭 socket，可能在读写回调中执行
        OnWriteStrand = 1, // 在写执行器中关闭 socket，保证与写循环不并发执行
    };

private:
    /** @brief 网络 IO 事件循环。 */
    boost::asio::io_context m_ioContext;

    /** @brief 业务回调事件循环。 */
    boost::asio::io_context m_taskContext;

    /** @brief TCP 套接字，仅应在写执行器序列中执行关闭/重建。 */
    tcp::socket m_socket;

    /** @brief 读侧串行执行器，保证读回调顺序一致。 */
    Strand m_readStrand;

    /** @brief 写侧串行执行器，保证连接/写入状态机串行推进。 */
    Strand m_writeStrand;

    /** @brief 读缓冲区，承接 async_read_some 数据。 */
    std::array<char, MAX_READ_LENGTH> m_readBuffer;

    /** @brief 写批次字符串存储，拥有发送数据生命周期。 */
    std::vector<std::string> m_batchMessages;

    /** @brief 发送缓冲视图，元素引用 m_batchMessages 内存。 */
    std::vector<boost::asio::const_buffer> m_batchViews;

    /** @brief 无锁发送队列，生产者为 send()，消费者为 startWriteLoop()。 */
    moodycamel::ConcurrentQueue<std::string> m_sendQueue;

    /** @brief IO 线程集合。 */
    Threads m_ioThreads;

    /** @brief 回调任务线程集合。 */
    Threads m_taskThreads;

    /** @brief 防止 m_ioContext 在无任务时提前退出。 */
    WorkGuardPtr m_ioWorkGuard;

    /** @brief 防止 m_taskContext 在无任务时提前退出。 */
    WorkGuardPtr m_taskWorkGuard;

    /** @brief IO 线程数配置值。 */
    size_t m_ioThreadCount = 0;

    /** @brief 回调线程数配置值。 */
    size_t m_taskThreadCount = 0;

    /** @brief 客户端生命周期状态（枚举化状态）。 */
    std::atomic<LifecycleState> m_lifecycleState{LifecycleState::Created};

    /** @brief 连接状态（枚举化状态）。 */
    std::atomic<ConnectionState> m_connectionState{ConnectionState::Idle};

    /** @brief 写泵是否正在运行。 */
    std::atomic_bool m_writePumpRunning{false};

    /** @brief 当前连接目标地址。 */
    NetAddr m_connectTarget;

    /** @brief 自动重连控制器。 */
    CReconnectController m_reconnectController;

    /** @brief 用户回调集合，所有调用均投递到 taskContext。 */
    ClientCallback m_callback;

private:
    // 核心异步操作

    /**
     * @brief 启动一次连接尝试。
     * @details 该方法在写执行器上下文中运行。
     */
    void beginConnect() noexcept;

    /**
     * @brief 连接完成回调。
     * @param ec 连接结果错误码。
     */
    void onConnectResult(boost::system::error_code ec) noexcept;

    /**
     * @brief 启动异步读循环。
     */
    void startReadLoop() noexcept;

    /**
     * @brief 启动或续跑写循环。
     */
    void startWriteLoop() noexcept;

    /**
     * @brief 停止客户端并回收线程资源。
     */
    void stop() noexcept;

    // 回调报告
    void reportConnected() noexcept;
    void reportDisconnected() noexcept;
    void reportMessageReceived(const char* data, size_t length) noexcept;
    void reportError(const std::string& msg) noexcept;

    // 线程投递辅助
    void postWrite(std::function<void(Impl&)> task) noexcept;
    void postTask(std::function<void(Impl&)> task) noexcept;

    // 写流程辅助

    /**
     * @brief 尝试启动写循环闸门。
     * @return 成功抢占返回 true。
     */
    bool tryStartWritePump() noexcept;

    /**
     * @brief 从队列批量提取发送数据。
     */
    bool dequeueWriteBatch(size_t& count) noexcept;

    /**
     * @brief 清理写循环占用标记。
     */
    void finishWritePump() noexcept;

    // 连接状态辅助

    /**
     * @brief 统一处理断链路径。
     * @param mode 断链后的收敛模式。
     * @param closeMode socket 关闭执行模式。
     */
    void processDisconnect(DisconnectMode mode, SocketCloseMode closeMode) noexcept;

    /**
     * @brief 安排一次重连尝试。
     * @details 该方法会在写执行器中回到 beginConnect()。
     */
    void scheduleReconnectAttempt() noexcept;

    /**
     * @brief 立即关闭 socket。
     */
    void closeSocketNow() noexcept;

private:
    // 线程初始化辅助
    bool initThreads(size_t threadCount,
                     boost::asio::io_context& context,
                     WorkGuardPtr& workGuard,
                     Threads& threads) noexcept;
};

} // namespace asio
