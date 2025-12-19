#include "CAppRunUtil.h"

#include "CConfigLoader.h"

#include "CEventDispatcherFactory.h"

#include <iostream>

int main()
{
    // 读取配置文件
    const std::string configDir = "resource";
    if (!CConfigLoader::loadConfigDirectory(configDir)) {
        std::cerr << "Failed to load configuration files from directory: " << configDir
                  << std::endl;
        return -1;
    }

    std::cout << "> Application is running. Press Ctrl+C to exit. <" << std::endl;

    // 获取任务管理器实例
    auto& appTaskManager = TaskManager::getInstance();

    // 创建事件分发器
    auto eventDispatcher = CEventDispatcherFactory::createEventDispatcher();
    if (eventDispatcher == nullptr) {
        std::cerr << "Failed to create Event Dispatcher." << std::endl;
        return -1;
    }

    // 运行应用
    appTaskManager.waitForAllTasks();
}
