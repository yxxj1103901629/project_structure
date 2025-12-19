// =========================================================================
// Module: session
// File: CSystemCmd.h
// Version: 1.0.0
// Description: 系统内部统一指令模型定义
// Author: Session Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

namespace business {
namespace session {
namespace cmd {

/**
 * @enum ECommandType
 * @brief 指令类型枚举，定义系统支持的指令类型
 */
enum class ECommandType {
    CMD_UNKNOWN = 0,                  ///< 未知指令
    CMD_DEVICE_CONTROL = 1,           ///< 设备控制指令
    CMD_DEVICE_QUERY = 2,             ///< 设备查询指令
    CMD_ACCESS_CONTROL = 3,           ///< 门禁控制指令
    CMD_ALARM_CONTROL = 4,            ///< 告警控制指令
    CMD_SYSTEM_CONFIG = 5,            ///< 系统配置指令
    CMD_SESSION_MANAGE = 6,           ///< 会话管理指令
    CMD_EMERGENCY_STOP = 7,           ///< 紧急停止指令
    CMD_HEARTBEAT = 8                 ///< 心跳指令
};

/**
 * @enum ECommandSource
 * @brief 指令来源枚举，定义指令的原始来源
 */
enum class ECommandSource {
    SOURCE_UNKNOWN = 0,               ///< 未知来源
    SOURCE_TCP_CONSOLE = 1,           ///< TCP操作台
    SOURCE_MQTT_JSON = 2,             ///< MQTT JSON格式
    SOURCE_HTTP_API = 3,              ///< HTTP API
    SOURCE_LOCAL_CONSOLE = 4,         ///< 本地控制台
    SOURCE_AUTOMATION = 5             ///< 自动化系统
};

/**
 * @class CSystemCmd
 * @brief 系统内部统一指令模型类
 * @details 封装系统内部使用的统一指令格式，包含指令类型、参数、来源和时间戳等信息
 */
class CSystemCmd {
public:
    /**
     * @brief 构造函数
     */
    CSystemCmd();
    
    /**
     * @brief 构造函数
     * @param cmdType 指令类型
     * @param source 指令来源
     */
    CSystemCmd(ECommandType cmdType, ECommandSource source);
    
    /**
     * @brief 析构函数
     */
    ~CSystemCmd();
    
    /**
     * @brief 设置指令类型
     * @param cmdType 指令类型
     */
    void setCommandType(ECommandType cmdType);
    
    /**
     * @brief 获取指令类型
     * @return ECommandType 指令类型
     */
    ECommandType getCommandType() const;
    
    /**
     * @brief 设置指令来源
     * @param source 指令来源
     */
    void setCommandSource(ECommandSource source);
    
    /**
     * @brief 获取指令来源
     * @return ECommandSource 指令来源
     */
    ECommandSource getCommandSource() const;
    
    /**
     * @brief 设置指令ID
     * @param cmdId 指令唯一标识
     */
    void setCommandId(const std::string& cmdId);
    
    /**
     * @brief 获取指令ID
     * @return std::string 指令唯一标识
     */
    std::string getCommandId() const;
    
    /**
     * @brief 设置参数
     * @param key 参数键
     * @param value 参数值
     */
    void setParameter(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取参数
     * @param key 参数键
     * @param[out] value 参数值
     * @return bool 参数是否存在
     */
    bool getParameter(const std::string& key, std::string& value) const;
    
    /**
     * @brief 检查参数是否存在
     * @param key 参数键
     * @return bool 参数是否存在
     */
    bool hasParameter(const std::string& key) const;
    
    /**
     * @brief 移除参数
     * @param key 参数键
     * @return bool 参数是否存在并被移除
     */
    bool removeParameter(const std::string& key);
    
    /**
     * @brief 获取所有参数
     * @return const std::unordered_map<std::string, std::string>& 参数映射表
     */
    const std::unordered_map<std::string, std::string>& getAllParameters() const;
    
    /**
     * @brief 清空所有参数
     */
    void clearParameters();
    
    /**
     * @brief 设置指令生成时间戳
     * @param timestamp 时间戳（毫秒）
     */
    void setTimestamp(uint64_t timestamp);
    
    /**
     * @brief 获取指令生成时间戳
     * @return uint64_t 时间戳（毫秒）
     */
    uint64_t getTimestamp() const;
    
    /**
     * @brief 设置原始指令数据
     * @param rawData 原始指令数据
     */
    void setRawData(const std::string& rawData);
    
    /**
     * @brief 获取原始指令数据
     * @return std::string 原始指令数据
     */
    std::string getRawData() const;
    
    /**
     * @brief 获取指令类型的字符串表示
     * @param cmdType 指令类型
     * @return std::string 字符串表示
     */
    static std::string commandTypeToString(ECommandType cmdType);
    
    /**
     * @brief 获取指令来源的字符串表示
     * @param source 指令来源
     * @return std::string 字符串表示
     */
    static std::string commandSourceToString(ECommandSource source);
    
    /**
     * @brief 将字符串转换为指令类型
     * @param typeStr 字符串表示
     * @return ECommandType 指令类型
     */
    static ECommandType stringToCommandType(const std::string& typeStr);
    
    /**
     * @brief 将字符串转换为指令来源
     * @param sourceStr 字符串表示
     * @return ECommandSource 指令来源
     */
    static ECommandSource stringToCommandSource(const std::string& sourceStr);
    
private:
    std::string m_commandId;           ///< 指令唯一标识
    ECommandType m_commandType;        ///< 指令类型
    ECommandSource m_commandSource;    ///< 指令来源
    std::unordered_map<std::string, std::string> m_parameters;  ///< 参数存储结构
    uint64_t m_timestamp;              ///< 指令生成时间戳（毫秒）
    std::string m_rawData;             ///< 原始指令数据
};

} // namespace cmd
} // namespace session
} // namespace business
