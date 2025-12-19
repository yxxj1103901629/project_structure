#ifndef CCONFIGLOADER_H
#define CCONFIGLOADER_H

#include "../../../common/config/CConfigManager.h"

using namespace OutSide::Common::Config;

// 功能包含：调用公共模块内的配置读写模块，参数在线配置方法等
class CConfigLoader
{
public:
    // 读取目录下所有配置文件
    static bool loadConfigDirectory(const std::string& dirPath)
    {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (!loadConfigFile(entry.path().string())) {
                    return false;
                }
            }
        }
        return true;
    }
    // 加载配置文件
    static bool loadConfigFile(const std::string& filePath)
    {
        return CConfigManager::getInstance().loadFromFile(filePath);
    }

    // 保存配置文件
    static bool saveConfigFile(const std::string& filePath)
    {
        return CConfigManager::getInstance().saveToFile(filePath);
    }

    // 获取配置值
    template<typename T>
    static T getConfigValue(const std::string& key, const T& defaultValue = T{})
    {
        return CConfigManager::getInstance().getValue<T>(key, defaultValue);
    }

    // 设置配置值
    template<typename T>
    static void setConfigValue(const std::string& key, const T& value)
    {
        CConfigManager::getInstance().setValue<T>(key, value);
    }
};

#endif // CCONFIGLOADER_H
