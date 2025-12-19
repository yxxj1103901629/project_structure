#include "CEventDispatcher.h"

#include "../../../common/network/impl/mqttClient/CPahoMqttClient.h"
#include "../../../common/network/impl/tcp/CUVTcpServer.h"

#include "CAppRunUtil.h"
#include "CBusinessDataUtil.h"
#include "CConfigLoader.h"
#include "CTimeUtil.h"


#include <iostream>

CEventDispatcher::CEventDispatcher()
    : m_loop(nullptr)
    , m_isInitialized(false)
{}

CEventDispatcher::~CEventDispatcher() {}

void initializeMainServer(const nlohmann::json& mainServerConfig)
{
    if (mainServerConfig.is_null()) {
        std::cerr << "主服务器配置为空。" << std::endl;
        return;
    }

    // 启动主TCP服务器
    using namespace OutSide::Common::Network;
    // 定义为静态变量以保持其生命周期
    static CUVTcpServer mainServer;
    // 先停止服务器以防止重复启动
    mainServer.stop();

    // 捕获mainServerConfig以确保其在任务执行时有效
    TaskManager::addTask(AppRunner::packageAppTask(
        [mainServerConfig]() {
            // 读取主服务器配置
            std::string host = mainServerConfig.value("host", "");
            int port = mainServerConfig.value("port", 0);

            // 设置服务器启动回调
            mainServer.setStartCallback([host, port](bool success, const std::string& msg) {
                if (success) {
                    std::cout << "主服务器已启动于 [" << host << ":" << port << "]" << std::endl;
                } else {
                    std::cout << "主服务器启动失败: " << msg << std::endl;
                }
            });
            // 设置服务器停止回调
            mainServer.setStopCallback([](const std::string& info) {
                std::cout << "主服务器已停止: " << info << std::endl;
            });
            // 设置客户端连接回调
            mainServer.setClientConnectCallback(
                [](const Address& clientAddr, bool success, const std::string& msg) {
                    if (success) {
                        std::cout << "客户端已连接: " << clientAddr.toString() << std::endl;
                    } else {
                        std::cout << "客户端连接失败: " << msg << std::endl;
                    }
                });
            // 启动服务器
            mainServer.listen(host, port);
        },
        [] {
            // 程序退出时停止服务器（可选）
            mainServer.stop();
        }));
}

void initializeMQTTClient(const nlohmann::json& mqttConfig)
{
    if (mqttConfig.is_null()) {
        std::cerr << "MQTT客户端配置为空。" << std::endl;
        return;
    }

    using namespace OutSide::Common::Network;

    // 定义为静态变量以保持其生命周期
    static CPahoMqttClient mqttClient;
    // 先断开连接以防止重复连接
    mqttClient.disconnect();

    // 捕获mqttConfig以确保其在任务执行时有效
    TaskManager::addTask(AppRunner::packageAppTask(
        [mqttConfig]() {
            // 读取MQTT客户端配置
            const std::string broker = mqttConfig.value("broker", "");
            const std::string clientId = mqttConfig.value("clientId", "");
            const std::string username = mqttConfig.value("username", "");
            const std::string password = mqttConfig.value("password", "");

            if (mqttClient.init(broker, clientId)) {
                // 设置连接回调
                mqttClient.setConnectCallback([=](bool success, const std::string& info) {
                    if (success) {
                        std::cout << "MQTT客户端 [" << broker << "] 连接成功。" << std::endl;

                        // 从配置文件内读取订阅配置
                        auto subscribes
                            = CConfigLoader::getConfigValue("mqtt.subscriptions.subscribe",
                                                            nlohmann::json{});
                        if (subscribes.is_array()) {
                            for (const auto& sub : subscribes) {
                                std::string topic = sub.value("topic", "");
                                int qos = sub.value("qos", 0);
                                if (!topic.empty()) {
                                    mqttClient.subscribe(topic, qos);
                                }
                            }
                        }
                        // 从配置文件内读取发布配置
                        auto publishes = CConfigLoader::getConfigValue("mqtt.publications.publish",
                                                                       nlohmann::json{});

                        if (publishes.is_array()) {
                            for (const auto& pub : publishes) {
                                std::string topic = pub.value("topic", "");
                                auto data = pub.value("payload", nlohmann::json{});

                                auto payload = CBusinessDataUtil::generateBusinessData(topic, data);
                                if (payload != "") {
                                    int qos = pub.value("qos", 0);
                                    bool retain = pub.value("retain", false);

                                    // std::cout << "\n================================" << std::endl;
                                    // std::cout << "准备发布MQTT消息:" << std::endl;
                                    // std::cout << "主题: " << topic << std::endl;
                                    // std::cout << "服务质量等级: " << qos << std::endl;
                                    // std::cout << "保留消息: " << (retain ? "是" : "否") << std::endl;
                                    // std::cout << "载荷内容: " << payload << std::endl;
                                    // std::cout << "================================\n" << std::endl;

                                    mqttClient.publish(topic, payload, qos, retain);
                                }
                            }
                        }

                    } else {
                        std::cout << "MQTT客户端连接[" << broker << "]失败: " << info << std::endl;
                    }
                });
                // 设置断开连接回调
                mqttClient.setDisconnectCallback([](bool success, const std::string& info) {
                    if (success) {
                        std::cout << "MQTT客户端断开连接成功。" << std::endl;
                    } else {
                        std::cout << "MQTT客户端断开连接失败: " << info << std::endl;
                    }
                });
                // 设置发布结果回调
                mqttClient.setPublishCallback([](bool success, const MqttCallbackInfo& info) {
                    if (success) {
                        std::cout << "MQTT消息发布到主题 [" << info.topic << "] 成功。"
                                  << std::endl;
                    } else {
                        std::cout << "MQTT消息发布失败: " << info.error << std::endl;
                    }
                });
                // 设置订阅结果回调
                mqttClient.setSubscribeCallback([](bool success, const MqttCallbackInfo& info) {
                    if (success) {
                        std::cout << "MQTT主题 [" << info.topic << "] 订阅成功。" << std::endl;

                    } else {
                        std::cout << "MQTT主题订阅失败: " << info.error << std::endl;
                    }
                });
                // 设置取消订阅结果回调
                mqttClient.setUnsubscribeCallback([](bool success, const MqttCallbackInfo& info) {
                    if (success) {
                        std::cout << "MQTT主题 [" << info.topic << "] 取消订阅成功。" << std::endl;
                    } else {
                        std::cout << "MQTT主题取消订阅失败: " << info.error << std::endl;
                    }
                });
                // 设置接收消息回调
                mqttClient.setRecvCallback([](const CMqttMessage& message) {
                    // std::cout << "\n===============================" << std::endl;
                    // std::cout << "时间: " << getCurrentTimeString() << std::endl;
                    // std::cout << "收到MQTT消息:" << std::endl;
                    // std::cout << "主题: " << message.topic << std::endl;
                    // std::cout << "载荷长度: " << message.payloadLen << std::endl;
                    // std::cout << "载荷内容: " << std::string(message.payload, message.payloadLen)
                    //           << std::endl;
                    // std::cout << "===============================\n" << std::endl;




                });
                // 连接到MQTT代理服务器
                mqttClient.connect(username, password);
            } else {
                std::cerr << "初始化MQTT客户端失败。" << std::endl;
            }
        },
        [] {
            // 程序退出时断开MQTT客户端连接（可选）
            mqttClient.disconnect();
        }));
}

bool CEventDispatcher::init()
{
    if (m_isInitialized) {
        return true;
    }

    if (!m_loop) {
        m_loop = OutSide::Common::EventLoop::CUVMultiloop::getInstance();
    }

    // 初始化主服务器
    // initializeMainServer(CConfigLoader::getConfigValue("tcpServers.main", nlohmann::json{}));



    auto mqttEvents = CConfigLoader::getConfigValue("events.mqtt", nlohmann::json{});
    if (mqttEvents.is_array()) {
        for (const auto& eventConfig : mqttEvents) {
            EventType eventType = even
        }
    }


    // 初始化MQTT客户端
    initializeMQTTClient(CConfigLoader::getConfigValue("mqtt", nlohmann::json{}));

    std::cout << "时间: " << getCurrentTimeString() << " - 事件分发器初始化完成。" << std::endl;

    m_isInitialized = true;
    return true;
}

void CEventDispatcher::registerEventCallback(EventType type, const EventData& eventData)
{
    (void)type;
    (void)eventData;
}

void CEventDispatcher::triggerEvent(const EventData& eventData)
{
    if (m_loop) {
        m_loop->postTask([eventData]() {
            // 在这里处理事件分发逻辑
            std::cout << "事件触发: 网络类型 = " << static_cast<int>(eventData.networkType)
                      << ", 事件类型 = " << static_cast<int>(eventData.networkEventType)
                      << ", 源地址 = " << eventData.source
                      << ", 目标地址 = " << eventData.destination
                      << ", 时间 = " << eventData.time << std::endl;
        });
    }
}
