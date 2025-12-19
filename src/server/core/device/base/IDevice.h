#ifndef IDEVICE_H
#define IDEVICE_H

#include <nlohmann/json.hpp>

class CDeviceAttribute;

class IDevice
{
public:
    virtual ~IDevice() = default;

    /**
     * @brief 初始化设备
     * @param config 设备配置参数，JSON格式
     */
    virtual void init(const nlohmann::json& config) = 0;

    /**
     * @brief 处理设备数据
     * @param data 设备数据，字符串格式
     */
    virtual void process(const std::string& data) = 0;

    /**
     * @brief 获取设备属性
     * @return 设备属性指针
     */
    virtual CDeviceAttribute* getDeviceAttribute() const = 0;


};

#endif // IDEVICE_H
