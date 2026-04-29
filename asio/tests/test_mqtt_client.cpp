#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <mqtt/CMosqMqttClient.h>
#include <windows.h>

using namespace mosq;
using namespace std::chrono_literals;

namespace {

bool expect(bool condition, const std::string& message)
{
    if (condition) {
        return true;
    }
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
}

Config makeConfig(const std::string& clientId)
{
    Config config;
    config.brokerAddress = "192.168.110.23";
    config.brokerPort = 2882;
    config.clientId = clientId;
    config.username = "1";
    config.password = "1";
    return config;
}

bool testMqttMultipleInstancesInitAndTeardown()
{
    auto client1 = std::make_unique<CMosqMqttClient>();
    auto client2 = std::make_unique<CMosqMqttClient>();

    if (!expect(client1->init(makeConfig("mqtt_multi_1")), "first mqtt client should init")) {
        return false;
    }
    if (!expect(client2->init(makeConfig("mqtt_multi_2")), "second mqtt client should init")) {
        return false;
    }

    client1.reset();
    client2.reset();
    return true;
}

bool testMqttConnectDisconnectIdempotence()
{
    CMosqMqttClient client;
    if (!expect(client.init(makeConfig("mqtt_idempotence")), "mqtt init should succeed")) {
        return false;
    }

    const bool firstConnect = client.connect();
    client.disconnect();
    client.disconnect();

    return expect(firstConnect, "first mqtt connect should start");
}

} // namespace

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    const struct
    {
        const char* name;
        bool (*fn)();
    } tests[] = {
        {"mqtt multiple instances init and teardown", testMqttMultipleInstancesInitAndTeardown},
        {"mqtt connect disconnect idempotence", testMqttConnectDisconnectIdempotence},
    };

    int failed = 0;
    for (const auto& t : tests) {
        std::cout << "[RUN ] " << t.name << std::endl;
        const bool ok = t.fn();
        if (ok) {
            std::cout << "[ OK ] " << t.name << std::endl;
        } else {
            ++failed;
            std::cout << "[FAIL] " << t.name << std::endl;
        }
    }

    if (failed == 0) {
        std::cout << "all mqtt tests passed" << std::endl;
        return 0;
    }

    std::cout << failed << " test(s) failed" << std::endl;
    return 1;
}
