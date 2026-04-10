#include "tcp/client/CReconnectController.h"

namespace {

constexpr auto RECONNECT_BASE_DELAY = std::chrono::milliseconds(200);
constexpr auto RECONNECT_MAX_DELAY = std::chrono::seconds(10);
constexpr uint32_t RECONNECT_MAX_EXPONENT = 20;

} // namespace

namespace asio {

CReconnectController::CReconnectController(boost::asio::io_context& ioContext,
                                           Strand& writeStrand) noexcept
    : m_timer(ioContext)
    , m_writeStrand(writeStrand)
{}

void CReconnectController::enable() noexcept
{
    m_policyState = PolicyState::Enabled;
}

void CReconnectController::disable() noexcept
{
    m_policyState = PolicyState::Disabled;
}

void CReconnectController::cancel() noexcept
{
    m_timer.cancel();
}

void CReconnectController::resetAttempt() noexcept
{
    m_attempt = 0;
}

void CReconnectController::schedule(std::function<void()> reconnectAction) noexcept
{
    if (m_policyState != PolicyState::Enabled) {
        return;
    }

    const auto delay = computeDelay();
    ++m_attempt;

    m_timer.expires_after(delay);
    m_timer.async_wait(
        boost::asio::bind_executor(m_writeStrand,
                                   [this, reconnectAction = std::move(reconnectAction)](
                                       const boost::system::error_code& ec) {
                                       if (ec) {
                                           return;
                                       }
                                       if (m_policyState != PolicyState::Enabled) {
                                           return;
                                       }
                                       reconnectAction();
                                   }));
}

CReconnectController::PolicyState CReconnectController::policyState() const noexcept
{
    return m_policyState;
}

std::chrono::milliseconds CReconnectController::computeDelay() const noexcept
{
    const auto exp = std::min<uint32_t>(m_attempt, RECONNECT_MAX_EXPONENT);
    auto delay = RECONNECT_BASE_DELAY * (1u << exp);
    if (delay > RECONNECT_MAX_DELAY) {
        delay = RECONNECT_MAX_DELAY;
    }
    return delay;
}

} // namespace asio
