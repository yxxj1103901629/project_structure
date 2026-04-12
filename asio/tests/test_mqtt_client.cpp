#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <mqtt/CMosqMqttClient.h>
#include <windows.h>

using namespace mosq;
using namespace std::chrono_literals;

namespace {

struct RuntimeState
{
    std::mutex mtx;
    std::atomic<int> connected{0};
    std::atomic<int> disconnected{0};
    std::atomic<int> subscribed{0};
    std::atomic<int> messages{0};
    std::atomic<int> errors{0};
};

} // namespace

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    CMosqMqttClient client;
    Config config;
    config.brokerAddress = "192.168.110.23";
    config.brokerPort = 2882;
    config.clientId = "mqtt_sub_test_ads_all";
    config.username = "1";
    config.password = "1";

    if (!client.init(config)) {
        std::cerr << "MQTT 客户端初始化失败" << std::endl;
        return 1;
    }

    RuntimeState state;

    ClientCallback cb;
    cb.connected = [&]() {
        ++state.connected;
        std::cout << "已连接到 broker，开始订阅 ads/#" << std::endl;
        if (!client.subscribe("ads/#", 0)) {
            std::cerr << "订阅 ads/# 失败" << std::endl;
        }
    };

    cb.disconnected = [&]() {
        ++state.disconnected;
        std::cout << "已断开连接" << std::endl;
    };

    cb.subscribed = [&](int mid, int qos) {
        ++state.subscribed;
        std::cout << "订阅成功, mid=" << mid << ", qos=" << qos << std::endl;
    };

    cb.messageReceived = [&](const std::string& topic, std::string_view payload) {
        ++state.messages;
        std::cout << "[MSG] topic=" << topic << ", payload=" << payload << std::endl;
    };

    cb.errorOccurred = [&](const std::string& err) {
        ++state.errors;
        std::cerr << "MQTT 错误: " << err << std::endl;
    };

    client.setCallback(std::move(cb));

    if (!client.connect()) {
        std::cerr << "连接 broker 失败: 192.168.110.23:2882" << std::endl;
        return 1;
    }

    std::cout << "MQTT 测试已启动，监听订阅 ads/#" << std::endl;
    std::cout << "将在 8 秒后自动退出..." << std::endl;

    std::this_thread::sleep_for(8s);

    std::cout << "MQTT 客户端停止中..." << std::endl;
    client.disconnect();
    std::this_thread::sleep_for(1s);

    const int connectedCount = state.connected.load();
    const int subscribedCount = state.subscribed.load();
    const int messageCount = state.messages.load();
    const int errorCount = state.errors.load();

    std::cout << "统计: connected=" << connectedCount << ", subscribed=" << subscribedCount
              << ", messages=" << messageCount << ", errors=" << errorCount << std::endl;

    if (connectedCount <= 0 || subscribedCount <= 0) {
        std::cerr << "测试失败: 未完成连接或订阅" << std::endl;
        return 1;
    }

    std::cout << "测试完成" << std::endl;
    return 0;
}