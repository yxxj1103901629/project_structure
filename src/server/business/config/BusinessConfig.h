#ifndef BUSINESSCONFIG_H
#define BUSINESSCONFIG_H

#include <string>
#include <unordered_map>

class BusinessConfig
{
public:
    // 定义一些常量
    static constexpr const char* SERVER_HOST = "127.0.0.1";
    static constexpr int SERVER_PORT = 40004;
    static constexpr const char* MQTT_HOST = "broker.example.com";
    static constexpr int MQTT_PORT = 1883;
    static constexpr const char* MQTT_USERNAME = "1";
    static constexpr const char* MQTT_PASSWORD = "1";
    static constexpr const char* MQTT_CLIENT_ID = "dmps_adsServer";

    // 默认MQTT业务数据ID映射
    inline static const std::unordered_map<std::string, int> DEFAULT_MQTT_BUSINESS_DATA = {
        {"unity/pathPlanningPlatform/topics", 1090101},
        {"unity/pathPlanningPlatform/templates", 1090201},
        {"unity/pathPlanningPlatform/templates/commands/feedback", 1050201},
        {"qt/ADSAVICS/topics", 1090101},
        {"qt/ADSAVICS/task/dispatch", 1030101},
    };
};

#endif // BUSINESSCONFIG_H
