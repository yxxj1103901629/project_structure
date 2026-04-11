#include "CMosqMqttClientImpl.h"
#include "utils/CAtomicUtil.h"
#include <cassert>
#include <thread>

namespace {

int PROTOCOL_VERSION = MQTT_PROTOCOL_V5; // MQTT5 版本

std::atomic<int> MID_COUNTER{0}; // 消息ID计数器，确保每条消息有唯一ID

constexpr size_t RECONNECT_INTERVAL_INIT_SEC = 5; // 初始重连间隔秒数
constexpr size_t RECONNECT_INTERVAL_MAX_SEC = 30; // 最大重连间隔秒数
constexpr size_t KEEP_ALIVE_INTERVAL_SEC = 60;    // 保持连接间隔秒数

} // namespace

namespace mosq {

CMosqMqttClient::Impl::Impl()
    : mosqpp::mosquittopp()
{}

CMosqMqttClient::Impl::~Impl()
{
    disconnect();
    mosqpp::lib_cleanup(); // 清理 mosquitto 库资源
}

bool CMosqMqttClient::Impl::init(const Config &config) noexcept
{
    if (CAtomicUtil::load(m_initialized)) {
        return false; // 已经初始化，防止重复初始化导致状态混乱
    }

    m_brokerAddress = config.brokerAddress;
    if (m_brokerAddress.empty()) {
        assert(false && "代理地址不能为空");
        return false;
    }
    m_brokerPort = config.brokerPort;
    if (m_brokerPort == 0) {
        assert(false && "代理端口必须大于0");
        return false;
    }
    m_clientId = config.clientId;
    if (m_clientId.empty()) {
        // 生成随机客户端ID，避免与其他客户端冲突
        m_clientId = "mosq_client_" + std::to_string(std::rand());
    }

    m_username = config.username;
    m_password = config.password;

    // 设置 MQTT 协议版本为 MQTT5
    if (opts_set(MOSQ_OPT_PROTOCOL_VERSION, &PROTOCOL_VERSION) != MOSQ_ERR_SUCCESS) {
        assert(false && "设置 MQTT 协议版本失败");
        return false;
    }

    // 初始化 mosquitto 库资源
    if (mosqpp::lib_init() != MOSQ_ERR_SUCCESS) {
        assert(false && "初始化 mosquitto 库失败");
        return false;
    }

    CAtomicUtil::store(m_initialized, true);
    return true;
}

bool CMosqMqttClient::Impl::connect() noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("客户端未初始化");
        return false;
    }

    if (m_state != State::DISCONNECTED) {
        reportError("客户端已连接或正在连接中");
        return false;
    }

    struct Guard
    {
        Impl *self;
        bool ok = false;
        Guard(Impl *impl)
            : self(impl)
        {
            // 在构造函数中设置状态为 CONNECTING，确保在连接过程中状态正确
            CAtomicUtil::store(self->m_state, State::CONNECTING);
        }
        ~Guard() noexcept
        {
            if (!ok) {
                // 如果连接失败，重置状态为 DISCONNECTED
                CAtomicUtil::store(self->m_state, State::DISCONNECTED);
            }
        }
        void release() noexcept { ok = true; }
    } gurad{this};

    // 设置客户端ID并清理会话
    if (reinitialise(m_clientId.c_str(), true) != MOSQ_ERR_SUCCESS) {
        reportError("设置客户端ID失败");
        return false;
    }

    // 设置用户名密码
    if (!m_username.empty()) {
        const auto &password = m_password.empty() ? nullptr : m_password.c_str();
        if (username_pw_set(m_username.c_str(), password) != MOSQ_ERR_SUCCESS) {
            reportError("设置用户名密码失败");
            return false;
        }
    }

    // 设置重连间隔
    reconnect_delay_set(RECONNECT_INTERVAL_INIT_SEC, // 最小重连间隔秒数
                        RECONNECT_INTERVAL_MAX_SEC,  // 最大重连间隔秒数
                        true                         // 启用指数退避
    );

    // 异步连接到代理
    int ret = connect_async(m_brokerAddress.c_str(), m_brokerPort, KEEP_ALIVE_INTERVAL_SEC);
    if (ret != MOSQ_ERR_SUCCESS) {
        reportError("连接到代理失败: " + std::to_string(ret));
        return false;
    }

    // 启用网络循环
    if (loop_start() != MOSQ_ERR_SUCCESS) {
        reportError("启动网络循环失败");
        return false;
    }

    // 流程到这里说明连接请求已成功发出，等待 on_connect_v5 回调确认连接结果
    gurad.release();

    return true;
}

void CMosqMqttClient::Impl::disconnect() noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        return; // 未初始化，无需断开连接
    }

    if (CAtomicUtil::load(m_state) == State::DISCONNECTED) {
        return; // 已经断开连接，无需重复断开
    }

    // 发送断开连接请求
    mosqpp::mosquittopp::disconnect();
    // 等待一段时间，确保断开连接请求被处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // 停止网络循环，等待 on_disconnect_v5 回调确认断开结果
    loop_stop(true);
}

bool CMosqMqttClient::Impl::publish(
    const std::string &topic, const char *payload, size_t payloadlen, int qos, bool retain) noexcept
{
    if (topic.empty()) {
        reportError("发布失败: 主题不能为空");
        return false;
    }

    if (qos < 0 || qos > 2) {
        reportError("发布失败: QoS必须为0、1或2");
        return false;
    }

    if (payload == nullptr) {
        reportError("发布失败: 有效载荷不能为空");
        return false;
    }

    if (payloadlen > 268435455 || payloadlen == 0) { // MQTT协议规定的最大有效载荷长度
        reportError("发布失败: 有效载荷过大或为空");
        return false;
    }

    if (!CAtomicUtil::load(m_initialized)) {
        reportError("客户端未初始化");
        return false;
    }

    if (CAtomicUtil::load(m_state) != State::CONNECTED) {
        reportError("客户端未连接");
        return false;
    }

    // 获取唯一的消息ID
    int mid = CAtomicUtil::exchange(MID_COUNTER, MID_COUNTER + 1);
    if (mosqpp::mosquittopp::publish(&mid,
                                     topic.c_str(),
                                     static_cast<int>(payloadlen),
                                     payload,
                                     qos,
                                     retain)
        != MOSQ_ERR_SUCCESS) {
        reportError("发布消息到主题" + topic + "失败");
        return false;
    }

    return true;
}

bool CMosqMqttClient::Impl::subscribe(const std::string &topic, int qos) noexcept
{
    if (topic.empty()) {
        reportError("订阅失败: 主题不能为空");
        return false;
    }

    if (qos < 0 || qos > 2) {
        reportError("订阅失败: QoS必须为0、1或2");
        return false;
    }

    if (!CAtomicUtil::load(m_initialized)) {
        reportError("客户端未初始化");
        return false;
    }

    if (CAtomicUtil::load(m_state) != State::CONNECTED) {
        reportError("客户端未连接");
        return false;
    }

    if (mosqpp::mosquittopp::subscribe(nullptr, topic.c_str(), qos) != MOSQ_ERR_SUCCESS) {
        reportError("订阅主题" + topic + "失败");
        return false;
    }

    return true;
}

bool CMosqMqttClient::Impl::unsubscribe(const std::string &topic) noexcept
{
    if (topic.empty()) {
        reportError("取消订阅失败: 主题不能为空");
        return false;
    }

    if (!CAtomicUtil::load(m_initialized)) {
        reportError("客户端未初始化");
        return false;
    }

    if (CAtomicUtil::load(m_state) != State::CONNECTED) {
        reportError("客户端未连接");
        return false;
    }

    if (mosqpp::mosquittopp::unsubscribe(nullptr, topic.c_str()) != MOSQ_ERR_SUCCESS) {
        reportError("取消订阅主题" + topic + "失败");
        return false;
    }

    return true;
}

void CMosqMqttClient::Impl::setCallback(const ClientCallback &callback) noexcept
{
    m_callback = callback;
}

void CMosqMqttClient::Impl::setCallback(ClientCallback &&callback) noexcept
{
    m_callback = std::move(callback);
}

void CMosqMqttClient::Impl::reportError(const std::string &msg) noexcept
{
    if (m_callback.errorOccurred) {
        m_callback.errorOccurred(msg);
    }
}

void CMosqMqttClient::Impl::reportConnected() noexcept
{
    if (m_callback.connected) {
        m_callback.connected();
    }
}

void CMosqMqttClient::Impl::reportDisconnected() noexcept
{
    if (m_callback.disconnected) {
        m_callback.disconnected();
    }
}

void CMosqMqttClient::Impl::reportMessageReceived(const std::string &topic,
                                                  std::string_view payload) noexcept
{
    if (m_callback.messageReceived) {
        m_callback.messageReceived(topic, payload);
    }
}

void CMosqMqttClient::Impl::reportSubscribed(int mid, int qos) noexcept
{
    if (m_callback.subscribed) {
        m_callback.subscribed(mid, qos);
    }
}

void CMosqMqttClient::Impl::reportUnsubscribed(int mid) noexcept
{
    if (m_callback.unsubscribed) {
        m_callback.unsubscribed(mid);
    }
}

void CMosqMqttClient::Impl::reportPublished(int mid) noexcept
{
    if (m_callback.published) {
        m_callback.published(mid);
    }
}

void CMosqMqttClient::Impl::on_connect_v5(int rc,
                                          int flags,
                                          const mosquitto_property *props) noexcept
{
    if (rc == 0) {
        CAtomicUtil::store(m_state, State::CONNECTED);
        reportConnected();
    } else {
        CAtomicUtil::store(m_state, State::DISCONNECTED);
        reportError("连接失败，错误代码: " + std::to_string(rc));
    }
}

void CMosqMqttClient::Impl::on_disconnect_v5(int rc, const mosquitto_property *props) noexcept
{
    CAtomicUtil::store(m_state, State::DISCONNECTED);
    if (rc == 0) {
        reportDisconnected();
    } else {
        reportError("意外断开连接，错误代码: " + std::to_string(rc));
    }
}

void CMosqMqttClient::Impl::on_message_v5(const struct mosquitto_message *message,
                                          const mosquitto_property *props) noexcept
{
    if (message == nullptr || message->topic == nullptr || message->payload == nullptr) {
        reportError("接收到无效消息");
        return;
    }

    std::string topic(message->topic);
    std::string_view payload(static_cast<const char *>(message->payload), message->payloadlen);
    reportMessageReceived(topic, payload);
}

void CMosqMqttClient::Impl::on_subscribe_v5(int mid,
                                            int qos_count,
                                            const int *granted_qos,
                                            const mosquitto_property *props) noexcept
{
    reportSubscribed(mid, (qos_count > 0) && (granted_qos != nullptr) ? granted_qos[0] : -1);
}

void CMosqMqttClient::Impl::on_unsubscribe_v5(int mid, const mosquitto_property *props) noexcept
{
    reportUnsubscribed(mid);
}

void CMosqMqttClient::Impl::on_publish_v5(int mid,
                                          int reason_code,
                                          const mosquitto_property *props) noexcept
{
    if (reason_code == 0) {
        reportPublished(mid);
    } else {
        reportError("发布消息失败，错误代码: " + std::to_string(reason_code));
    }
}

void CMosqMqttClient::Impl::on_error() noexcept
{
    reportError("发生未知错误");
}

void CMosqMqttClient::Impl::on_log(int level, const char *str) noexcept
{
    (void) level;
    (void) str;
}

} // namespace mosq