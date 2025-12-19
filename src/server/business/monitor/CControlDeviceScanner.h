// =========================================================================
// Module: monitor
// File: CControlDeviceScanner.h
// Version: 1.0.0
// Description: 控制设备扫描器类定义，提供控制设备报文监听与异常检测功能
// Author: Security Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明安全模块接口，减少头文件依赖
typedef struct _ExceptionEvent ExceptionEvent;

namespace business {
namespace monitor {

/**
 * @struct ControlDeviceMessage
 * @brief 控制设备报文结构体，存储原始报文及解析后的数据
 */
struct ControlDeviceMessage
{
    std::string deviceId;                                    ///< 发送设备ID
    std::string messageId;                                   ///< 报文唯一标识
    uint32_t messageType;                                    ///< 报文类型
    std::vector<uint8_t> rawData;                            ///< 原始报文数据
    std::unordered_map<std::string, std::string> parsedData; ///< 解析后的键值对数据
    uint64_t timestamp;                                      ///< 接收时间戳
};

/**
 * @enum ExceptionType
 * @brief 异常类型枚举，定义各类控制设备异常
 */
enum class ExceptionType {
    INVALID_FORMAT = 0,        ///< 报文格式无效
    DATA_OUT_OF_RANGE = 1,     ///< 数据超出有效范围
    UNKNOWN_DEVICE = 2,        ///< 未知设备发送
    FREQUENCY_OVER_LIMIT = 3,  ///< 发送频率超限
    INVALID_COMMAND = 4,       ///< 无效命令
    EMERGENCY_STOP = 5,        ///< 急停信号
    AUTHENTICATION_FAILED = 6, ///< 认证失败
    OTHER = 99                 ///< 其他异常
};

/**
 * @class CControlDeviceScanner
 * @brief 控制设备扫描器类，负责监听控制设备报文并进行异常检测
 */
class CControlDeviceScanner
{
public:
    /**
     * @brief 构造函数
     */
    CControlDeviceScanner();

    /**
     * @brief 析构函数
     */
    ~CControlDeviceScanner();

    /**
     * @brief 开始监听控制设备报文
     * @param port 监听端口
     * @param protocolType 协议类型（TCP/UDP等）
     * @return bool 监听是否成功启动
     */
    bool startListening(uint16_t port, const std::string& protocolType);

    /**
     * @brief 停止监听控制设备报文
     * @return bool 停止是否成功
     */
    bool stopListening();

    /**
     * @brief 处理接收到的控制设备报文
     * @param message 控制设备报文
     * @return bool 处理是否成功
     * @note 会自动调用异常检测算法并推送异常事件
     */
    bool processMessage(const ControlDeviceMessage& message);

    /**
     * @brief 验证报文格式是否有效
     * @param message 控制设备报文
     * @param[out] errorDetails 错误详情（如果验证失败）
     * @return bool 格式是否有效
     */
    bool validateMessageFormat(const ControlDeviceMessage& message, std::string& errorDetails) const;

    /**
     * @brief 检查报文中的数据是否在有效范围内
     * @param message 控制设备报文
     * @param[out] invalidFields 无效字段列表
     * @return bool 数据是否全部在有效范围内
     */
    bool checkDataRange(const ControlDeviceMessage& message,
                        std::vector<std::string>& invalidFields) const;

    /**
     * @brief 检测发送频率是否超限
     * @param deviceId 设备ID
     * @return bool 是否超限
     */
    bool detectFrequencyOverLimit(const std::string& deviceId);

    /**
     * @brief 设置异常事件回调
     * @param callback 异常事件回调函数
     * @note 用于将异常事件推送至安全控制子模块
     */
    void setExceptionEventCallback(std::function<void(const ExceptionEvent&)> callback);

    /**
     * @brief 添加允许的设备ID列表
     * @param allowedDevices 允许的设备ID列表
     */
    void addAllowedDevices(const std::vector<std::string>& allowedDevices);

    /**
     * @brief 移除允许的设备ID
     * @param deviceId 要移除的设备ID
     * @return bool 是否成功移除
     */
    bool removeAllowedDevice(const std::string& deviceId);

    /**
     * @brief 设置频率限制参数
     * @param deviceId 设备ID（空字符串表示全局设置）
     * @param maxMessages 最大消息数
     * @param timeWindowSeconds 时间窗口（秒）
     */
    void setFrequencyLimit(const std::string& deviceId,
                           uint32_t maxMessages,
                           uint32_t timeWindowSeconds);

private:
    /**
     * @brief 检测异常并生成异常事件
     * @param message 原始报文
     * @param exceptionType 异常类型
     * @param details 异常详情
     * @return bool 是否成功生成并推送事件
     */
    bool detectAndGenerateException(const ControlDeviceMessage& message,
                                    ExceptionType exceptionType,
                                    const std::string& details);

    /**
     * @brief 更新设备发送统计信息
     * @param deviceId 设备ID
     */
    void updateDeviceStatistics(const std::string& deviceId);

    std::unordered_map<std::string, bool> m_allowedDevices;         ///< 允许的设备ID列表
    std::function<void(const ExceptionEvent&)> m_exceptionCallback; ///< 异常事件回调
    mutable std::mutex m_mutex;                                     ///< 线程安全互斥锁

    // 频率限制相关数据结构
    struct FrequencyInfo
    {
        uint32_t messageCount;    ///< 消息计数
        uint64_t windowStartTime; ///< 窗口开始时间
    };
    std::unordered_map<std::string, FrequencyInfo> m_frequencyMap; ///< 设备频率统计
    uint32_t m_globalMaxMessages;                                  ///< 全局最大消息数
    uint32_t m_globalTimeWindow;                                   ///< 全局时间窗口（秒）

    bool m_isListening; ///< 监听状态标记
};

} // namespace monitor
} // namespace business
