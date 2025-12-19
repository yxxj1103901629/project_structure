// =========================================================================
// Module: security
// File: CControlDeviceEmergencyStrategy.h
// Version: 1.0.0
// Description: 控制端急停策略类定义，处理控制设备急停事件的安全策略
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
 * @enum EmergencyStopLevel
 * @brief 急停级别枚举，定义急停信号的紧急程度
 */
enum class EmergencyStopLevel {
    LEVEL_1 = 0, ///< 一级急停（局部区域）
    LEVEL_2 = 1, ///< 二级急停（区域）
    LEVEL_3 = 2, ///< 三级急停（全局）
    LEVEL_4 = 3  ///< 四级急停（灾难级别）
};

/**
 * @enum EmergencyResponseMeasure
 * @brief 紧急响应措施枚举，定义不同急停级别的响应措施
 */
enum class EmergencyResponseMeasure {
    STOP_EQUIPMENT = 0,              ///< 停止相关设备
    LOCK_ACCESS_CONTROL = 1,         ///< 锁定门禁控制
    ACTIVATE_EMERGENCY_ALARM = 2,    ///< 激活紧急告警
    NOTIFY_EMERGENCY_TEAM = 3,       ///< 通知应急团队
    CLOSE_FIRE_SYSTEM = 4,           ///< 启动消防系统
    SHUTDOWN_POWER = 5,              ///< 切断电源
    ACTIVATE_ESCALATION = 6,         ///< 启动逐级响应
    CONTACT_EXTERNAL_AUTHORITIES = 7 ///< 联系外部机构
};

/**
 * @struct EmergencyStopPolicy
 * @brief 急停策略结构，定义急停级别与响应措施的对应关系
 */
struct EmergencyStopPolicy
{
    EmergencyStopLevel stopLevel;                   ///< 急停级别
    std::vector<EmergencyResponseMeasure> measures; ///< 响应措施列表
    SecurityLevel securityLevel;                    ///< 安全级别
    std::vector<std::string> affectedSystems;       ///< 受影响系统列表
    bool requiresManualReset;                       ///< 是否需要手动复位
    uint32_t responseTimeoutMs;                     ///< 响应超时时间（毫秒）
};

/**
 * @struct EmergencySignal
 * @brief 急停信号结构，定义急停信号的格式
 */
struct EmergencySignal
{
    std::string signalId;         ///< 信号唯一标识
    std::string sourceDeviceId;   ///< 信号源设备ID
    EmergencyStopLevel stopLevel; ///< 急停级别
    std::string signalType;       ///< 信号类型
    bool isConfirmed;             ///< 是否已确认
    uint64_t timestamp;           ///< 信号生成时间戳
};

/**
 * @class CControlDeviceEmergencyStrategy
 * @brief 控制端急停策略类，继承ISecurityStrategy接口，处理控制设备急停事件
 */
class CControlDeviceEmergencyStrategy : public ISecurityStrategy
{
public:
    /**
     * @brief 构造函数
     */
    CControlDeviceEmergencyStrategy();

    /**
     * @brief 析构函数
     */
    ~CControlDeviceEmergencyStrategy() override;

    /**
     * @brief 执行控制端急停策略
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
     * @brief 处理急停信号
     * @param emergencySignal 急停信号
     * @return bool 处理是否成功
     */
    bool processEmergencySignal(const EmergencySignal& emergencySignal);

    /**
     * @brief 执行紧急联动逻辑
     * @param stopLevel 急停级别
     * @param sourceDeviceId 信号源设备ID
     * @param[out] executedMeasures 实际执行的措施列表
     * @return bool 执行是否成功
     */
    bool executeEmergencyLinkage(EmergencyStopLevel stopLevel,
                                 const std::string& sourceDeviceId,
                                 std::vector<std::string>& executedMeasures);

    /**
     * @brief 确认急停信号
     * @param signalId 信号ID
     * @return bool 确认是否成功
     */
    bool confirmEmergencySignal(const std::string& signalId);

    /**
     * @brief 复位急停状态
     * @param stopLevel 急停级别
     * @param sourceDeviceId 信号源设备ID（空字符串表示全局复位）
     * @return bool 复位是否成功
     */
    bool resetEmergencyState(EmergencyStopLevel stopLevel, const std::string& sourceDeviceId = "");

    /**
     * @brief 获取当前急停状态
     * @param[out] activeStops 当前激活的急停列表
     * @return uint32_t 当前激活的急停数量
     */
    uint32_t getCurrentEmergencyState(std::vector<EmergencySignal>& activeStops) const;

    /**
     * @brief 添加急停策略配置
     * @param policy 急停策略
     */
    void addEmergencyPolicy(const EmergencyStopPolicy& policy);

    /**
     * @brief 更新急停策略配置
     * @param stopLevel 急停级别
     * @param policy 新的急停策略
     * @return bool 更新是否成功
     */
    bool updateEmergencyPolicy(EmergencyStopLevel stopLevel, const EmergencyStopPolicy& policy);

    /**
     * @brief 获取急停策略配置
     * @param stopLevel 急停级别
     * @param[out] policy 急停策略
     * @return bool 获取是否成功
     */
    bool getEmergencyPolicy(EmergencyStopLevel stopLevel, EmergencyStopPolicy& policy) const;

private:
    /**
     * @brief 将字符串急停级别转换为枚举
     * @param levelStr 急停级别字符串
     * @return EmergencyStopLevel 急停级别枚举
     */
    EmergencyStopLevel stringToStopLevel(const std::string& levelStr) const;

    /**
     * @brief 停止相关设备
     * @param affectedSystems 受影响系统列表
     * @return bool 停止是否成功
     */
    bool stopRelatedEquipment(const std::vector<std::string>& affectedSystems);

    /**
     * @brief 启动逐级响应机制
     * @param stopLevel 急停级别
     * @param sourceDeviceId 信号源设备ID
     * @return bool 启动是否成功
     */
    bool activateEscalationProcedure(EmergencyStopLevel stopLevel,
                                     const std::string& sourceDeviceId);

    std::unordered_map<EmergencyStopLevel, EmergencyStopPolicy> m_emergencyPolicyMap; ///< 急停策略映射
    std::vector<EmergencySignal> m_activeEmergencySignals; ///< 当前激活的急停信号列表
    mutable std::mutex m_mutex;                            ///< 线程安全互斥锁
};

} // namespace security
} // namespace business
