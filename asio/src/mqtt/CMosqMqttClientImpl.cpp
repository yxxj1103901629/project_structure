#include "CMosqMqttClientImpl.h"
#include "utils/CAtomicUtil.h"
#include <cassert>

namespace {

int PROTOCOL_VERSION = MQTT_PROTOCOL_V5;          // MQTT5 版本
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

    return true;
}

void CMosqMqttClient::Impl::disconnect() noexcept {}

bool CMosqMqttClient::Impl::publish(const std::string &topic, const std::string &payload) noexcept
{}

bool CMosqMqttClient::Impl::subscribe(const std::string &topic, int qos) noexcept {}

bool CMosqMqttClient::Impl::unsubscribe(const std::string &topic) noexcept {}

void CMosqMqttClient::Impl::setCallback(const ClientCallback &callback) noexcept {}

void CMosqMqttClient::Impl::setCallback(ClientCallback &&callback) noexcept {}

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
{}

void CMosqMqttClient::Impl::on_disconnect_v5(int rc, const mosquitto_property *props) noexcept {}

void CMosqMqttClient::Impl::on_message_v5(const struct mosquitto_message *message,
                                          const mosquitto_property *props) noexcept
{}

void CMosqMqttClient::Impl::on_subscribe_v5(int mid,
                                            int qos_count,
                                            const int *granted_qos,
                                            const mosquitto_property *props) noexcept
{}

void CMosqMqttClient::Impl::on_unsubscribe_v5(int mid, const mosquitto_property *props) noexcept {}

void CMosqMqttClient::Impl::on_publish_v5(int mid,
                                          int reason_code,
                                          const mosquitto_property *props) noexcept
{}

void CMosqMqttClient::Impl::on_error() noexcept {}

void CMosqMqttClient::Impl::on_log(int level, const char *str) noexcept {}

} // namespace mosq