#include "CMosqMqttClientImpl.h"
#include "utils/CAtomicUtil.h"

#include <cassert>
#include <cstdlib>

namespace {

int PROTOCOL_VERSION = MQTT_PROTOCOL_V5;
std::atomic<int> MID_COUNTER{1};
std::atomic<int> MOSQ_LIB_USERS{0};
std::mutex MOSQ_LIB_MUTEX;
bool MOSQ_LIB_READY = false;
std::mutex MQTT_TASK_MUTEX;
std::unique_ptr<boost::asio::io_context> MQTT_TASK_CONTEXT;
std::unique_ptr<asio::WorkGuard> MQTT_TASK_GUARD;
std::unique_ptr<std::thread> MQTT_TASK_THREAD;

constexpr size_t RECONNECT_INTERVAL_INIT_SEC = 5;
constexpr size_t RECONNECT_INTERVAL_MAX_SEC = 30;
constexpr size_t KEEP_ALIVE_INTERVAL_SEC = 60;

bool retainMosqLibrary() noexcept
{
    std::lock_guard lock(MOSQ_LIB_MUTEX);
    if (!MOSQ_LIB_READY) {
        if (mosqpp::lib_init() != MOSQ_ERR_SUCCESS) {
            return false;
        }
        MOSQ_LIB_READY = true;
    }
    MOSQ_LIB_USERS.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

void releaseMosqLibrary() noexcept
{
    std::lock_guard lock(MOSQ_LIB_MUTEX);
    (void)MOSQ_LIB_USERS.fetch_sub(1, std::memory_order_acq_rel);
}

struct MosqLibraryProcessCleanup
{
    ~MosqLibraryProcessCleanup()
    {
        {
            std::lock_guard lock(MQTT_TASK_MUTEX);
            if (MQTT_TASK_GUARD) {
                MQTT_TASK_GUARD.reset();
            }
            if (MQTT_TASK_CONTEXT) {
                MQTT_TASK_CONTEXT->stop();
            }
        }
        if (MQTT_TASK_THREAD && MQTT_TASK_THREAD->joinable()) {
            MQTT_TASK_THREAD->join();
        }
        MQTT_TASK_THREAD.reset();
        MQTT_TASK_CONTEXT.reset();

        std::lock_guard lock(MOSQ_LIB_MUTEX);
        if (MOSQ_LIB_READY) {
            mosqpp::lib_cleanup();
            MOSQ_LIB_READY = false;
        }
    }
};

MosqLibraryProcessCleanup MOSQ_LIB_PROCESS_CLEANUP;

class MosqLibraryLease
{
public:
    bool acquire() noexcept
    {
        if (m_acquired) {
            return true;
        }
        m_acquired = retainMosqLibrary();
        return m_acquired;
    }

    void release() noexcept
    {
        if (!m_acquired) {
            return;
        }
        releaseMosqLibrary();
        m_acquired = false;
    }

    ~MosqLibraryLease()
    {
        release();
    }

private:
    bool m_acquired{false};
};

bool ensureMqttTaskDispatcher() noexcept
{
    std::lock_guard lock(MQTT_TASK_MUTEX);
    if (MQTT_TASK_CONTEXT) {
        return true;
    }

    auto context = std::make_unique<boost::asio::io_context>();
    auto guard = std::make_unique<asio::WorkGuard>(boost::asio::make_work_guard(*context));
    auto thread = std::make_unique<std::thread>([ctx = context.get()] {
        try {
            ctx->run();
        } catch (...) {
        }
    });

    MQTT_TASK_CONTEXT = std::move(context);
    MQTT_TASK_GUARD = std::move(guard);
    MQTT_TASK_THREAD = std::move(thread);
    return true;
}

} // namespace

namespace mosq {

CMosqMqttClient::Impl::Impl()
    : mosqpp::mosquittopp()
{}

CMosqMqttClient::Impl::~Impl()
{
    disconnect();
    if (CAtomicUtil::exchange(m_initialized, false)) {
        releaseMosqLibrary();
    }
}

bool CMosqMqttClient::Impl::init(const Config& config) noexcept
{
    bool expected = false;
    if (!m_initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;
    }

    struct Guard
    {
        Impl* self;
        bool ok{false};
        MosqLibraryLease libraryLease;
        ~Guard()
        {
            if (!ok) {
                CAtomicUtil::store(self->m_initialized, false);
            }
        }
        void release() noexcept { ok = true; }
    } guard{this};

    if (!guard.libraryLease.acquire()) {
        return false;
    }

    if (!ensureMqttTaskDispatcher()) {
        return false;
    }
    m_brokerAddress = config.brokerAddress;
    if (m_brokerAddress.empty()) {
        return false;
    }

    m_brokerPort = config.brokerPort;
    if (m_brokerPort == 0) {
        return false;
    }

    m_clientId = config.clientId.empty() ? "mosq_client_" + std::to_string(std::rand()) : config.clientId;
    m_username = config.username;
    m_password = config.password;

    if (opts_set(MOSQ_OPT_PROTOCOL_VERSION, &PROTOCOL_VERSION) != MOSQ_ERR_SUCCESS) {
        return false;
    }

    guard.libraryLease.release();
    guard.release();
    return true;
}

bool CMosqMqttClient::Impl::connect() noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("mqtt client is not initialized");
        return false;
    }

    State expected = State::Disconnected;
    if (!m_state.compare_exchange_strong(expected, State::Connecting, std::memory_order_acq_rel)) {
        reportError("mqtt client is already connecting or connected");
        return false;
    }

    struct Guard
    {
        Impl* self;
        bool ok{false};
        ~Guard()
        {
            if (!ok) {
                CAtomicUtil::store(self->m_state, State::Disconnected);
            }
        }
        void release() noexcept { ok = true; }
    } guard{this};

    if (reinitialise(m_clientId.c_str(), true) != MOSQ_ERR_SUCCESS) {
        reportError("failed to reinitialize mosquitto client");
        return false;
    }

    if (!m_username.empty()) {
        const char* password = m_password.empty() ? nullptr : m_password.c_str();
        if (username_pw_set(m_username.c_str(), password) != MOSQ_ERR_SUCCESS) {
            reportError("failed to set mqtt credentials");
            return false;
        }
    }

    reconnect_delay_set(RECONNECT_INTERVAL_INIT_SEC, RECONNECT_INTERVAL_MAX_SEC, true);

    const int connectResult = connect_async(m_brokerAddress.c_str(), m_brokerPort, KEEP_ALIVE_INTERVAL_SEC);
    if (connectResult != MOSQ_ERR_SUCCESS) {
        reportError("failed to start async mqtt connect: " + std::to_string(connectResult));
        return false;
    }

    const int loopResult = loop_start();
    if (loopResult != MOSQ_ERR_SUCCESS) {
        reportError("failed to start mqtt network loop");
        return false;
    }
    CAtomicUtil::store(m_loopStarted, true);

    guard.release();
    return true;
}

void CMosqMqttClient::Impl::disconnect() noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        return;
    }

    const auto previousState = CAtomicUtil::exchange(m_state, State::Disconnected);
    if (previousState != State::Disconnected) {
        mosqpp::mosquittopp::disconnect();
    }

    if (CAtomicUtil::exchange(m_loopStarted, false)) {
        loop_stop(true);
    }
}

bool CMosqMqttClient::Impl::publish(const std::string& topic,
                                    const char* payload,
                                    size_t payloadlen,
                                    int qos,
                                    bool retain) noexcept
{
    if (topic.empty() || payload == nullptr || payloadlen == 0 || payloadlen > 268435455) {
        reportError("publish rejected due to invalid topic or payload");
        return false;
    }
    if (qos < 0 || qos > 2) {
        reportError("publish rejected due to invalid qos");
        return false;
    }
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("mqtt client is not initialized");
        return false;
    }
    if (CAtomicUtil::load(m_state) != State::Connected) {
        reportError("mqtt client is not connected");
        return false;
    }

    int mid = MID_COUNTER.fetch_add(1, std::memory_order_relaxed);
    if (mosqpp::mosquittopp::publish(&mid,
                                     topic.c_str(),
                                     static_cast<int>(payloadlen),
                                     payload,
                                     qos,
                                     retain)
        != MOSQ_ERR_SUCCESS) {
        reportError("failed to publish topic: " + topic);
        return false;
    }

    return true;
}

bool CMosqMqttClient::Impl::subscribe(const std::string& topic, int qos) noexcept
{
    if (topic.empty()) {
        reportError("subscribe rejected due to empty topic");
        return false;
    }
    if (qos < 0 || qos > 2) {
        reportError("subscribe rejected due to invalid qos");
        return false;
    }
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("mqtt client is not initialized");
        return false;
    }
    if (CAtomicUtil::load(m_state) != State::Connected) {
        reportError("mqtt client is not connected");
        return false;
    }

    if (mosqpp::mosquittopp::subscribe(nullptr, topic.c_str(), qos) != MOSQ_ERR_SUCCESS) {
        reportError("failed to subscribe topic: " + topic);
        return false;
    }

    return true;
}

bool CMosqMqttClient::Impl::unsubscribe(const std::string& topic) noexcept
{
    if (topic.empty()) {
        reportError("unsubscribe rejected due to empty topic");
        return false;
    }
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("mqtt client is not initialized");
        return false;
    }
    if (CAtomicUtil::load(m_state) != State::Connected) {
        reportError("mqtt client is not connected");
        return false;
    }

    if (mosqpp::mosquittopp::unsubscribe(nullptr, topic.c_str()) != MOSQ_ERR_SUCCESS) {
        reportError("failed to unsubscribe topic: " + topic);
        return false;
    }

    return true;
}

void CMosqMqttClient::Impl::setCallback(const ClientCallback& callback) noexcept
{
    std::lock_guard lock(m_callbackMutex);
    m_callback = callback;
}

void CMosqMqttClient::Impl::setCallback(ClientCallback&& callback) noexcept
{
    std::lock_guard lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void CMosqMqttClient::Impl::reportError(const std::string& msg) noexcept
{
    postTask([msg](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.errorOccurred) {
            callback.errorOccurred(msg);
        }
    });
}

void CMosqMqttClient::Impl::reportConnected() noexcept
{
    postTask([](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.connected) {
            callback.connected();
        }
    });
}

void CMosqMqttClient::Impl::reportDisconnected() noexcept
{
    postTask([](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.disconnected) {
            callback.disconnected();
        }
    });
}

void CMosqMqttClient::Impl::reportMessageReceived(const std::string& topic, std::string payload) noexcept
{
    postTask([topic, payload = std::move(payload)](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.messageReceived) {
            callback.messageReceived(topic, std::string_view(payload));
        }
    });
}

void CMosqMqttClient::Impl::reportSubscribed(int mid, int qos) noexcept
{
    postTask([mid, qos](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.subscribed) {
            callback.subscribed(mid, qos);
        }
    });
}

void CMosqMqttClient::Impl::reportUnsubscribed(int mid) noexcept
{
    postTask([mid](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.unsubscribed) {
            callback.unsubscribed(mid);
        }
    });
}

void CMosqMqttClient::Impl::reportPublished(int mid) noexcept
{
    postTask([mid](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.published) {
            callback.published(mid);
        }
    });
}

ClientCallback CMosqMqttClient::Impl::copyCallback() const noexcept
{
    std::lock_guard lock(m_callbackMutex);
    return m_callback;
}

void CMosqMqttClient::Impl::postTask(std::function<void(Impl&)> task) noexcept
{
    std::lock_guard lock(MQTT_TASK_MUTEX);
    if (!MQTT_TASK_CONTEXT) {
        return;
    }
    boost::asio::post(*MQTT_TASK_CONTEXT, [this, task = std::move(task)]() mutable { task(*this); });
}

void CMosqMqttClient::Impl::on_connect_v5(int rc, int, const mosquitto_property*) noexcept
{
    if (rc == 0) {
        CAtomicUtil::store(m_state, State::Connected);
        reportConnected();
    } else {
        CAtomicUtil::store(m_state, State::Disconnected);
        reportError("mqtt connect failed, rc=" + std::to_string(rc));
    }
}

void CMosqMqttClient::Impl::on_disconnect_v5(int rc, const mosquitto_property*) noexcept
{
    CAtomicUtil::store(m_state, State::Disconnected);
    if (rc == 0) {
        reportDisconnected();
    } else {
        reportError("mqtt disconnected unexpectedly, rc=" + std::to_string(rc));
    }
}

void CMosqMqttClient::Impl::on_message_v5(const struct mosquitto_message* message,
                                          const mosquitto_property*) noexcept
{
    if (message == nullptr || message->topic == nullptr || message->payload == nullptr) {
        reportError("received invalid mqtt message");
        return;
    }

    std::string topic(message->topic);
    std::string payload(static_cast<const char*>(message->payload), static_cast<size_t>(message->payloadlen));
    reportMessageReceived(topic, std::move(payload));
}

void CMosqMqttClient::Impl::on_subscribe_v5(int mid,
                                            int qos_count,
                                            const int* granted_qos,
                                            const mosquitto_property*) noexcept
{
    reportSubscribed(mid, (qos_count > 0 && granted_qos != nullptr) ? granted_qos[0] : -1);
}

void CMosqMqttClient::Impl::on_unsubscribe_v5(int mid, const mosquitto_property*) noexcept
{
    reportUnsubscribed(mid);
}

void CMosqMqttClient::Impl::on_publish_v5(int mid, int reason_code, const mosquitto_property*) noexcept
{
    if (reason_code == 0) {
        reportPublished(mid);
    } else {
        reportError("mqtt publish failed, rc=" + std::to_string(reason_code));
    }
}

void CMosqMqttClient::Impl::on_error() noexcept
{
    reportError("mqtt internal error");
}

void CMosqMqttClient::Impl::on_log(int level, const char* str) noexcept
{
    (void)level;
    (void)str;
}

} // namespace mosq
