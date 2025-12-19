#include <iostream>
#include <string>

#include "common/util/CAppRunUtil.h"
#include "common/util/CTimeUtil.h"

#include "CPahoMqttClient.h"
#include "CUVTcpClient.h"
#include "CUVTcpServer.h"

#include "CConfigManager.h"
Common::Config::CConfigManager& g_ConfigManager = Common::Config::CConfigManager::getInstance();

// 连接到定位服务器
Common::Network::CUVTcpClient g_ToPositioningServerClient;
auto connectToPositioningServer(Common::Network::CUVTcpClient& client)
{
    const std::string& host = g_ConfigManager.getValue<std::string>("positionServer.host");
    const int port = g_ConfigManager.getValue<int>("positionServer.port");

    // 设置连接回调
    client.setConnectCallback([host, port](bool success, const std::string& error) {
        if (success) {
            std::cout << "Connected to server [" << host << ":" << port << "] successfully."
                      << std::endl;
        } else {
            std::cout << "Failed to connect to server: " << error << std::endl;
        }
    });
    // 设置断开回调
    client.setDisconnectCallback([](bool success, const std::string& error) {
        if (success) {
            std::cout << "Disconnected from server successfully.\n";
        } else {
            std::cout << "Failed to disconnect from server: " << error << std::endl;
        }
    });
    // 设置接收回调
    client.setReceiveCallback([=](const char* data, size_t length) {
        std::string receivedData(data, length);
        std::cout << "[" << Common::Util::getCurrentTimeString()
                  << "] Received data from server: " << length << std::endl;
    });

    // 包装为应用任务
    return Common::Util::AppRunner::packageAppTask(
        [&client, host, port]() {
            // 连接到服务器
            client.connect(host, port);
        },
        [&client]() {
            // 断开连接
            client.disconnect();
        });
}

// TCP服务器
Common::Network::CUVTcpServer g_TcpServer;
auto startTcpServer(Common::Network::CUVTcpServer& server)
{
    const std::string& host = g_ConfigManager.getValue<std::string>("server.host");
    const int port = g_ConfigManager.getValue<int>("server.port");

    // 设置服务器启动回调
    server.setStartCallback([host, port](bool success, const std::string& info) {
        if (success) {
            std::cout << "TCP server started on [" << host << ":" << port << "]" << std::endl;
        } else {
            std::cout << "Failed to start TCP server: " << info << std::endl;
        }
    });
    // 设置服务器停止回调
    server.setStopCallback(
        [](const std::string& info) { std::cout << "TCP server stopped: " << info << "\n"; });
    // 设置客户端连接回调
    server.setConnectCallback([&](const Address& addr, bool success, const std::string& error) {
        if (success) {
            std::cout << "Client connected: " << addr.toString() << std::endl;

            // 发送测试数据
            std::string testData = "Hello from TCP server!\n";
            server.send(addr, testData);
        } else {
            std::cout << "Failed to accept client connection from " << addr.toString() << ": "
                      << error << std::endl;
        }
    });
    // 设置客户端断开连接回调
    server.setDisconnectCallback([](const Address& addr) {
        std::cout << "Client disconnected: " << addr.toString() << std::endl;
    });
    // 设置客户端接收数据回调
    server.setReceiveCallback([](const Address& addr, const std::string& data) {
        std::cout << "Received data from " << addr.toString() << ": " << data << std::endl;
    });

    // 包装为应用任务
    return Common::Util::AppRunner::packageAppTask(
        [&server, host, port]() {
            // 启动TCP服务器
            server.listen(host, port);
        },
        []() {});
}

// 连接MQTT服务器
Common::Network::CPahoMqttClient g_MqttClient;
auto connectToMqttServer(Common::Network::CPahoMqttClient& client)
{
    const std::string& host = g_ConfigManager.getValue<std::string>("mqtt.host");
    const int port = g_ConfigManager.getValue<int>("mqtt.port");
    const std::string& username = g_ConfigManager.getValue<std::string>("mqtt.username");
    const std::string& password = g_ConfigManager.getValue<std::string>("mqtt.password");
    const std::string& clientId = g_ConfigManager.getValue<std::string>("mqtt.clientId");
    const std::string& topic = g_ConfigManager.getValue<std::string>("mqtt.topic");

    // 设置连接回调
    client.setConnectCallback([&client, host, port, topic](bool success, const std::string& info) {
        if (success) {
            std::cout << "Connected to MQTT broker [" << host << ":" << port << "] successfully."
                      << std::endl;

            int qos = 0;

            // 订阅主题
            client.subscribe(topic, qos);

            // 发布测试消息
            std::string payload = "Hello from Qt MQTT Client!";
            client.publish(topic, payload, qos, false);

        } else {
            std::cout << "Failed to connect to MQTT broker: " << info << std::endl;
        }
    });
    // 设置断开回调
    client.setDisconnectCallback([](bool success, const std::string& info) {
        if (success) {
            std::cout << "Disconnected from MQTT broker successfully.\n";
        } else {
            std::cout << "Failed to disconnect from MQTT broker: " << info << std::endl;
        }
    });
    // 设置消息接收回调
    // client.setRecvCallback([](const Common::Network::CMqttMessage& message) {
    //     std::string payload(message.payload, message.payload + message.payloadLen);
    //     std::cout << "\n=== Received MQTT Message ===" << std::endl;
    //     std::cout << "Topic: " << message.topic << std::endl;
    //     std::cout << "Payload: " << payload << std::endl;
    //     std::cout << "QoS: " << message.qos << std::endl;
    //     std::cout << "Retained: " << (message.retained ? "Yes" : "No") << std::endl;
    //     std::cout << "=============================\n" << std::endl;
    // });

    // 包装为应用任务
    return Common::Util::AppRunner::packageAppTask(
        [&client, host, port, username, password, clientId]() {
            // 连接到MQTT服务器
            if (client.init("tcp://" + host + ":" + std::to_string(port), clientId)) {
                client.connect(username, password);
            } else {
                std::cout << "Failed to initialize MQTT client." << std::endl;
            }
        },
        [&client]() {
            // 断开MQTT连接
            client.disconnect();
        });
}

int main()
{
    // 读取配置文件
    if (g_ConfigManager.loadFromFile("test_main.json")) {
        std::cout << "Configuration file loaded successfully." << std::endl;
    } else {
        std::cout << "Failed to load configuration file." << std::endl;
        return -1;
    }

    std::cout << " >Application is running. Press Ctrl+C to exit.<" << std::endl;

    // 添加任务到任务管理器
    Common::Util::TaskManager::addTasks(connectToPositioningServer(g_ToPositioningServerClient),
                                        startTcpServer(g_TcpServer),
                                        connectToMqttServer(g_MqttClient),
                                        [] { return 0; } // 保持主任务运行
    );

    // 运行应用
    Common::Util::TaskManager::waitForAllTasks();
}
