// =========================================================================
// Module: monitor
// File: COnlineDeviceRegistry.h
// Version: 1.0.0
// Description: 设备在线注册器类定义，提供设备管理与超时监控功能
// Author: Security Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明安全模块接口，减少头文件依赖
typedef struct _OfflineEvent OfflineEvent;

namespace business {
namespace monitor {

/**
 * @class CDeviceInfo
 * @brief 设备信息结构体，存储设备实例及状态信息
 */
struct CDeviceInfo
{
    void* deviceInstance;                                 ///< 设备实例指针
    std::chrono::steady_clock::time_point lastActiveTime; ///< 最后活跃时间
    std::string deviceId;                                 ///< 设备唯一标识
    std::string deviceType;                               ///< 设备类型
    bool isOnline;                                        ///< 在线状态标记
};

/**
 * @class COnlineDeviceRegistry
 * @brief 设备在线注册器类，负责设备的注册、注销和超时监控
 */
class COnlineDeviceRegistry
{
public:
    /**
     * @brief 构造函数
     * @param timeoutSeconds 设备超时时间（秒），默认30秒
     */
    explicit COnlineDeviceRegistry(uint32_t timeoutSeconds = 30);

    /**
     * @brief 析构函数
     */
    ~COnlineDeviceRegistry();

    /**
     * @brief 注册设备
     * @param deviceId 设备唯一标识
     * @param deviceInstance 设备实例指针
     * @param deviceType 设备类型
     * @return bool 注册是否成功
     * @note 线程安全，支持重复注册（会更新最后活跃时间）
     */
    bool registerDevice(const std::string& deviceId,
                        void* deviceInstance,
                        const std::string& deviceType);

    /**
     * @brief 注销设备
     * @param deviceId 设备唯一标识
     * @return bool 注销是否成功
     * @note 线程安全
     */
    bool deregisterDevice(const std::string& deviceId);

    /**
     * @brief 更新设备活跃时间
     * @param deviceId 设备唯一标识
     * @return bool 更新是否成功
     * @note 线程安全，通常由设备心跳机制调用
     */
    bool updateDeviceActivity(const std::string& deviceId);

    /**
     * @brief 获取设备信息
     * @param deviceId 设备唯一标识
     * @param[out] deviceInfo 设备信息结构体
     * @return bool 获取是否成功
     * @note 线程安全
     */
    bool getDeviceInfo(const std::string& deviceId, CDeviceInfo& deviceInfo) const;

    /**
     * @brief 获取所有在线设备列表
     * @param[out] onlineDevices 在线设备列表
     * @return uint32_t 在线设备数量
     * @note 线程安全
     */
    uint32_t getOnlineDevices(std::vector<CDeviceInfo>& onlineDevices) const;

    /**
     * @brief 检查所有设备的超时状态
     * @return uint32_t 本次检查发现的离线设备数量
     * @note 线程安全，会自动触发离线事件
     */
    uint32_t checkTimeoutDevices();

    /**
     * @brief 设置设备离线事件回调
     * @param callback 离线事件回调函数
     * @note 线程安全，用于将离线事件推送至安全控制子模块
     */
    void setOfflineEventCallback(std::function<void(const OfflineEvent&)> callback);

    /**
     * @brief 设置超时时间
     * @param timeoutSeconds 超时时间（秒）
     * @note 线程安全
     */
    void setTimeoutSeconds(uint32_t timeoutSeconds);

    /**
     * @brief 获取当前超时时间
     * @return uint32_t 超时时间（秒）
     */
    uint32_t getTimeoutSeconds() const;

private:
    /**
     * @brief 标记设备为离线状态
     * @param deviceId 设备唯一标识
     * @note 内部方法，会触发离线事件
     */
    void markDeviceOffline(const std::string& deviceId);

    std::unordered_map<std::string, CDeviceInfo> m_deviceMap;   ///< 设备注册表
    mutable std::mutex m_mutex;                                 ///< 线程安全互斥锁
    uint32_t m_timeoutSeconds;                                  ///< 设备超时时间（秒）
    std::function<void(const OfflineEvent&)> m_offlineCallback; ///< 离线事件回调
};

} // namespace monitor
} // namespace business
