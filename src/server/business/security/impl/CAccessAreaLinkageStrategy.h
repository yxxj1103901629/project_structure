// =========================================================================
// Module: security
// File: CAccessAreaLinkageStrategy.h
// Version: 1.0.0
// Description: 门禁红区联动策略类定义，处理门禁红区事件的安全策略
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
 * @enum AccessAreaType
 * @brief 门禁区域类型枚举，定义不同安全级别的门禁区域
 */
enum class AccessAreaType {
    RED_ZONE = 0,    ///< 红区（最高安全级别）
    ORANGE_ZONE = 1, ///< 橙区（较高安全级别）
    YELLOW_ZONE = 2, ///< 黄区（中等安全级别）
    GREEN_ZONE = 3,  ///< 绿区（较低安全级别）
    BLUE_ZONE = 4    ///< 蓝区（最低安全级别）
};

/**
 * @enum AccessAreaStatus
 * @brief 门禁区域状态枚举，定义区域当前状态
 */
enum class AccessAreaStatus {
    NORMAL = 0,     ///< 正常状态
    ALARM = 1,      ///< 告警状态
    EMERGENCY = 2,  ///< 紧急状态
    LOCKED = 3,     ///< 锁定状态
    MAINTENANCE = 4 ///< 维护状态
};

/**
 * @enum LinkageAction
 * @brief 联动动作枚举，定义红区事件触发的联动措施
 */
enum class LinkageAction {
    LOCK_ALL_ACCESS = 0,             ///< 锁定所有相关门禁
    ACTIVATE_VIDEO_SURVEILLANCE = 1, ///< 激活视频监控
    TRIGGER_ALARM = 2,               ///< 触发告警
    NOTIFY_SECURITY_PERSONNEL = 3,   ///< 通知安保人员
    CLOSE_FIRE_DOORS = 4,            ///< 关闭防火门
    ACTIVATE_EMERGENCY_LIGHTING = 5, ///< 激活应急照明
    RESTRICT_ACCESS_TO_ADJACENT = 6, ///< 限制相邻区域访问
    CALL_EMERGENCY_SERVICES = 7      ///< 呼叫应急服务
};

/**
 * @struct AreaLinkagePolicy
 * @brief 区域联动策略结构，定义红区状态与联动动作的映射关系
 */
struct AreaLinkagePolicy
{
    AccessAreaType areaType;                ///< 区域类型
    AccessAreaStatus triggerStatus;         ///< 触发状态
    std::vector<LinkageAction> actions;     ///< 联动动作列表
    SecurityLevel securityLevel;            ///< 安全级别
    std::vector<std::string> affectedAreas; ///< 受影响区域列表
    uint32_t actionDelayMs;                 ///< 动作延迟时间（毫秒）
};

/**
 * @struct ControlCommand
 * @brief 跨模块控制指令结构，定义生成的控制指令格式
 */
struct ControlCommand
{
    std::string commandId;                                      ///< 指令唯一标识
    std::string targetModule;                                   ///< 目标模块
    std::string targetId;                                       ///< 目标ID（如设备ID、区域ID）
    std::string commandType;                                    ///< 指令类型
    std::unordered_map<std::string, std::string> commandParams; ///< 指令参数
    uint64_t timestamp;                                         ///< 指令生成时间戳
};

/**
 * @class CAccessAreaLinkageStrategy
 * @brief 门禁红区联动策略类，继承ISecurityStrategy接口，处理门禁红区事件
 */
class CAccessAreaLinkageStrategy : public ISecurityStrategy
{
public:
    /**
     * @brief 构造函数
     */
    CAccessAreaLinkageStrategy();

    /**
     * @brief 析构函数
     */
    ~CAccessAreaLinkageStrategy() override;

    /**
     * @brief 执行门禁红区联动策略
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
     * @brief 监听红区状态变化
     * @param areaId 区域ID
     * @param currentStatus 当前状态
     * @param previousStatus 之前状态
     * @return bool 监听是否成功
     */
    bool monitorRedZoneStatus(const std::string& areaId,
                              AccessAreaStatus currentStatus,
                              AccessAreaStatus previousStatus);

    /**
     * @brief 生成跨模块控制指令
     * @param areaId 区域ID
     * @param areaType 区域类型
     * @param actions 联动动作列表
     * @return std::vector<ControlCommand> 生成的控制指令列表
     */
    std::vector<ControlCommand> generateCrossModuleCommands(
        const std::string& areaId,
        AccessAreaType areaType,
        const std::vector<LinkageAction>& actions);

    /**
     * @brief 执行区域联动动作
     * @param commands 控制指令列表
     * @param[out] executedCommands 实际执行的指令列表
     * @return bool 执行是否成功
     */
    bool executeAreaLinkageActions(const std::vector<ControlCommand>& commands,
                                   std::vector<std::string>& executedCommands);

    /**
     * @brief 获取受影响区域列表
     * @param triggerAreaId 触发区域ID
     * @param areaType 区域类型
     * @return std::vector<std::string> 受影响区域列表
     */
    std::vector<std::string> getAffectedAreas(const std::string& triggerAreaId,
                                              AccessAreaType areaType) const;

    /**
     * @brief 添加区域联动策略配置
     * @param policy 区域联动策略
     */
    void addLinkagePolicy(const AreaLinkagePolicy& policy);

    /**
     * @brief 更新区域联动策略配置
     * @param areaType 区域类型
     * @param triggerStatus 触发状态
     * @param policy 新的区域联动策略
     * @return bool 更新是否成功
     */
    bool updateLinkagePolicy(AccessAreaType areaType,
                             AccessAreaStatus triggerStatus,
                             const AreaLinkagePolicy& policy);

    /**
     * @brief 获取区域联动策略配置
     * @param areaType 区域类型
     * @param triggerStatus 触发状态
     * @param[out] policy 区域联动策略
     * @return bool 获取是否成功
     */
    bool getLinkagePolicy(AccessAreaType areaType,
                          AccessAreaStatus triggerStatus,
                          AreaLinkagePolicy& policy) const;

private:
    /**
     * @brief 将字符串区域类型转换为枚举
     * @param areaTypeStr 区域类型字符串
     * @return AccessAreaType 区域类型枚举
     */
    AccessAreaType stringToAreaType(const std::string& areaTypeStr) const;

    /**
     * @brief 将字符串区域状态转换为枚举
     * @param statusStr 区域状态字符串
     * @return AccessAreaStatus 区域状态枚举
     */
    AccessAreaStatus stringToAreaStatus(const std::string& statusStr) const;

    /**
     * @brief 发送控制指令到目标模块
     * @param command 控制指令
     * @return bool 发送是否成功
     */
    bool sendControlCommand(const ControlCommand& command);

    // 区域联动策略映射，使用复合键（区域类型+触发状态）
    using PolicyKey = std::pair<AccessAreaType, AccessAreaStatus>;
    std::unordered_map<PolicyKey, AreaLinkagePolicy> m_linkagePolicyMap;
    mutable std::mutex m_mutex; ///< 线程安全互斥锁

    // 区域与相邻区域的映射关系
    std::unordered_map<std::string, std::vector<std::string>> m_areaAdjacencyMap;
};

} // namespace security
} // namespace business
