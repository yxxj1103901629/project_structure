#pragma once

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstdint>
#include <functional>

namespace asio {

/**
 * @brief TCP 客户端重连调度控制器。
 * @details 该类仅负责重连退避和定时调度，不承载连接状态机。
 * @note 所有 schedule/cancel/enable/disable 调用应在写执行器上下文中进行。
 */
class CReconnectController
{
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    /**
     * @brief 重连策略开关状态。
     */
    enum class PolicyState : uint8_t {
        Disabled = 0,
        Enabled = 1,
    };

    /**
     * @brief 构造重连控制器。
     * @param ioContext 绑定的 io_context。
     * @param writeStrand 写侧串行执行器。
     * @details 定时器回调会绑定到 writeStrand，保证与连接状态机串行。
     */
    CReconnectController(boost::asio::io_context& ioContext, Strand& writeStrand) noexcept;

    /**
     * @brief 启用自动重连策略。
     */
    void enable() noexcept;

    /**
     * @brief 禁用自动重连策略。
     */
    void disable() noexcept;

    /**
     * @brief 取消当前已安排的重连定时器。
     */
    void cancel() noexcept;

    /**
     * @brief 重置重连次数计数。
     */
    void resetAttempt() noexcept;

    /**
     * @brief 按指数退避安排下一次重连动作。
     * @param reconnectAction 定时触发后执行的重连动作。
     * @details 若策略为 Disabled，则该调用不会安排任何定时器。
     */
    void schedule(std::function<void()> reconnectAction) noexcept;

    /**
     * @brief 获取当前策略状态。
     */
    PolicyState policyState() const noexcept;

private:
    /**
     * @brief 计算当前重连延迟。
     */
    std::chrono::milliseconds computeDelay() const noexcept;

private:
    PolicyState m_policyState{PolicyState::Disabled};
    uint32_t m_attempt{0};
    boost::asio::steady_timer m_timer;
    Strand& m_writeStrand;
};

} // namespace asio
