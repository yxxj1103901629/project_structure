// =========================================================================
// Module: security
// File: ISecurityStrategy.h
// Version: 1.0.0
// Description: 安全策略接口类定义，提供标准化的安全策略执行接口
// Author: Security Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace business {
namespace security {

/**
 * @enum EventSourceType
 * @brief 事件源类型枚举，定义事件来源
 */
enum class EventSourceType {
    MONITOR_DEVICE_OFFLINE = 0,           ///< 监控模块-设备离线
    MONITOR_CONTROL_DEVICE_EXCEPTION = 1, ///< 监控模块-控制设备异常
    ACCESS_CONTROL_RED_ZONE = 2,          ///< 门禁系统-红区事件
    VIDEO_SURVEILLANCE_ALARM = 3,         ///< 视频监控-告警
    OTHER = 99                            ///< 其他来源
};

/**
 * @enum SecurityLevel
 * @brief 安全级别枚举，定义策略执行的紧急程度
 */
enum class SecurityLevel {
    LOW = 0,      ///< 低级别
    MEDIUM = 1,   ///< 中级别
    HIGH = 2,     ///< 高级别
    EMERGENCY = 3 ///< 紧急级别
};

/**
 * @struct SecurityEventData
 * @brief 安全事件数据结构，统一事件信息传递格式
 */
struct SecurityEventData
{
    std::string eventId;                                       ///< 事件唯一标识
    EventSourceType sourceType;                                ///< 事件源类型
    SecurityLevel securityLevel;                               ///< 安全级别
    std::string sourceId;                                      ///< 事件源ID（如设备ID、区域ID等）
    std::string eventType;                                     ///< 事件类型描述
    std::unordered_map<std::string, std::string> eventDetails; ///< 事件详细信息
    uint64_t timestamp;                                        ///< 事件发生时间戳
};

/**
 * @struct StrategyExecutionResult
 * @brief 策略执行结果结构，定义策略执行后的返回信息
 */
struct StrategyExecutionResult
{
    bool success;                             ///< 执行是否成功
    std::string resultCode;                   ///< 结果代码
    std::string resultMessage;                ///< 结果描述
    std::vector<std::string> executedActions; ///< 执行的动作列表
    uint64_t executionTime;                   ///< 执行时间（毫秒）
};

/**
 * @class ISecurityStrategy
 * @brief 安全策略接口类，定义标准化的安全策略执行接口
 */
class ISecurityStrategy
{
public:
    /**
     * @brief 虚析构函数，确保派生类正确析构
     */
    virtual ~ISecurityStrategy() = default;

    /**
     * @brief 策略执行纯虚方法，所有安全策略必须实现此方法
     * @param eventData 安全事件数据
     * @param[out] result 执行结果
     * @return bool 执行是否成功启动（注意：返回true不代表执行成功完成，具体结果看result参数）
     */
    virtual bool execute(const SecurityEventData& eventData, StrategyExecutionResult& result) = 0;

    /**
     * @brief 获取策略名称
     * @return std::string 策略名称
     */
    virtual std::string getStrategyName() const = 0;

    /**
     * @brief 获取策略支持的事件类型
     * @return std::vector<std::string> 支持的事件类型列表
     */
    virtual std::vector<std::string> getSupportedEventTypes() const = 0;

    /**
     * @brief 检查策略是否支持特定事件
     * @param eventType 事件类型
     * @return bool 是否支持
     */
    virtual bool isSupportedEventType(const std::string& eventType) const;
};

// 定义监控模块需要使用的事件结构别名
typedef SecurityEventData OfflineEvent;
typedef SecurityEventData ExceptionEvent;

} // namespace security
} // namespace business
