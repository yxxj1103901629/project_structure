// =========================================================================
// Module: security
// File: CDeviceOfflineStrategy.h
// Version: 1.0.0
// Description: 设备离线策略类定义，处理设备离线事件的安全策略
// Author: Security Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include "business/security/base/ISecurityStrategy.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace business {
namespace security {

/**
 * @enum DeviceType
 * @brief 设备类型枚举，定义系统支持的设备类型
 */
enum class DeviceType {
    CONTROL_PANEL = 0,       ///< 操作台
    ACCESS_CONTROLLER = 1,   ///< 门禁控制器
    VIDEO_CAMERA = 2,        ///< 摄像头
    ALARM_SENSOR = 3,        ///< 报警传感器
    ENVIRONMENT_MONITOR = 4, ///< 环境监控器
    GATE_CONTROLLER = 5,     ///< 闸机控制器
    OTHER = 99               ///< 其他设备
};

/**
 * @enum OfflineProcessingAction
 * @brief 离线处理动作枚举，定义设备离线时的处理措施
 */
enum class OfflineProcessingAction {
    UPDATE_ROUTING = 0,    ///< 更新路由策略
    SEND_NOTIFICATION = 1, ///< 发送通知
    TRIGGER_BACKUP = 2,    ///< 触发备份设备
    LOCK_ACCESS = 3,       ///< 锁定相关门禁
    RAISE_ALARM = 4,       ///< 触发告警
    ESCALATE_SECURITY = 5  ///< 提升安全级别
};

/**
 * @struct DeviceOfflinePolicy
 * @brief 设备离线策略结构，定义设备类型与处理动作的映射关系
 */
struct DeviceOfflinePolicy
{
    DeviceType deviceType;                        ///< 设备类型
    std::vector<OfflineProcessingAction> actions; ///< 处理动作列表
    SecurityLevel securityLevel;                  ///< 安全级别
    bool requiresManualIntervention;              ///< 是否需要人工干预
    std::string backupDeviceType;                 ///< 备份设备类型
};

/**
 * @class CDeviceOfflineStrategy
 * @brief 设备离线策略类，继承ISecurityStrategy接口，处理设备离线事件
 */
class CDeviceOfflineStrategy : public ISecurityStrategy
{
public:
    /**
     * @brief 构造函数
     */
    CDeviceOfflineStrategy();

    /**
     * @brief 析构函数
     */
    ~CDeviceOfflineStrategy() override;

    /**
     * @brief 执行设备离线策略
     * @param eventData 安全事件数据
     * @param[out] result 执行结果
     * @return bool 执行是否成功启动
     */
    bool execute(const SecurityEventData& eventData, StrategyExecutionResult& result) override;

    /**
     * @brief 获取策略名称
     * @return std::string 策略名称
     */
    std::string getStrategyName() const override;

    /**
     * @brief 获取策略支持的事件类型
     * @return std::vector<std::string> 支持的事件类型列表
     */
    std::vector<std::string> getSupportedEventTypes() const override;

    /**
     * @brief 判断设备类型
     * @param deviceId 设备ID
     * @param deviceType 设备类型描述字符串
     * @return DeviceType 设备类型枚举
     */
    DeviceType determineDeviceType(const std::string& deviceId, const std::string& deviceType) const;

    /**
     * @brief 更新路由策略
     * @param deviceId 离线设备ID
     * @param deviceType 设备类型
     * @param[out] updatedRoutes 更新的路由列表
     * @return bool 更新是否成功
     */
    bool updateRoutingStrategy(const std::string& deviceId,
                               DeviceType deviceType,
                               std::vector<std::string>& updatedRoutes);

    /**
     * @brief 发送离线通知
     * @param deviceId 离线设备ID
     * @param deviceType 设备类型
     * @param securityLevel 安全级别
     * @return bool 发送是否成功
     */
    bool sendOfflineNotification(const std::string& deviceId,
                                 DeviceType deviceType,
                                 SecurityLevel securityLevel);

    /**
     * @brief 触发备份设备
     * @param primaryDeviceId 主设备ID
     * @param backupDeviceType 备份设备类型
     * @return std::string 激活的备份设备ID
     */
    std::string triggerBackupDevice(const std::string& primaryDeviceId,
                                    const std::string& backupDeviceType);

    /**
     * @brief 添加设备离线策略配置
     * @param policy 设备离线策略
     */
    void addOfflinePolicy(const DeviceOfflinePolicy& policy);

    /**
     * @brief 更新设备离线策略配置
     * @param deviceType 设备类型
     * @param policy 新的设备离线策略
     * @return bool 更新是否成功
     */
    bool updateOfflinePolicy(DeviceType deviceType, const DeviceOfflinePolicy& policy);

    /**
     * @brief 获取设备离线策略配置
     * @param deviceType 设备类型
     * @param[out] policy 设备离线策略
     * @return bool 获取是否成功
     */
    bool getOfflinePolicy(DeviceType deviceType, DeviceOfflinePolicy& policy) const;

private:
    /**
     * @brief 将字符串设备类型转换为枚举
     * @param deviceTypeStr 设备类型字符串
     * @return DeviceType 设备类型枚举
     */
    DeviceType stringToDeviceType(const std::string& deviceTypeStr) const;

    /**
     * @brief 执行设备离线处理动作
     * @param deviceId 设备ID
     * @param deviceType 设备类型
     * @param actions 处理动作列表
     * @param[out] executedActions 实际执行的动作列表
     * @return bool 执行是否成功
     */
    bool executeActions(const std::string& deviceId,
                        DeviceType deviceType,
                        const std::vector<OfflineProcessingAction>& actions,
                        std::vector<std::string>& executedActions);

    std::unordered_map<DeviceType, DeviceOfflinePolicy> m_devicePolicyMap; ///< 设备类型与处理策略的映射
    mutable std::mutex m_mutex;                                            ///< 线程安全互斥锁
};

} // namespace security
} // namespace business
