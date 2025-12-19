// =========================================================================
// Module: session
// File: CSessionManager.h
// Version: 1.0.0
// Description: 会话管理器核心实现
// Author: Session Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include <chrono>
#include "cmd/base/CSystemCmd.h"
#include "cmd/base/IConsoleCmdParser.h"
#include "cmd/factory/CConsoleCmdParserFactory.h"

namespace business {
namespace session {

/**
 * @struct SessionInfo
 * @brief 会话信息结构体
 * @details 包含会话的基本信息，如会话ID、操作台ID、绑定设备列表等
 */
struct SessionInfo {
    std::string sessionId;               ///< 会话ID
    std::string consoleId;               ///< 操作台ID
    std::vector<std::string> boundDevices; ///< 绑定的设备列表
    std::chrono::steady_clock::time_point lastActiveTime; ///< 最后活跃时间
    bool isActive;                       ///< 会话是否活跃
    cmd::ECommandSource consoleType;     ///< 操作台类型
    
    SessionInfo() : isActive(false), consoleType(cmd::ECommandSource::SOURCE_UNKNOWN) {}
};

/**
 * @enum CommandResultCode
 * @brief 指令执行结果码枚举
 */
enum class CommandResultCode {
    RESULT_SUCCESS = 0,                  ///< 执行成功
    RESULT_FAILURE = 1,                  ///< 执行失败
    RESULT_INVALID_COMMAND = 2,          ///< 无效指令
    RESULT_PERMISSION_DENIED = 3,        ///< 权限拒绝
    RESULT_DEVICE_OFFLINE = 4,           ///< 设备离线
    RESULT_SESSION_TIMEOUT = 5,          ///< 会话超时
    RESULT_INTERNAL_ERROR = 6            ///< 内部错误
};

/**
 * @struct CommandResult
 * @brief 指令执行结果结构体
 */
struct CommandResult {
    CommandResultCode resultCode;        ///< 结果码
    std::string resultMessage;           ///< 结果消息
    std::string commandId;               ///< 指令ID
    std::string sessionId;               ///< 会话ID
    
    CommandResult() : resultCode(CommandResultCode::RESULT_FAILURE) {}
};

/**
 * @class CSessionManager
 * @brief 会话管理器核心类
 * @details 实现会话管理的核心功能，包括操作台-设备绑定、指令处理流程、会话状态管理等
 */
class CSessionManager {
public:
    /**
     * @brief 构造函数
     */
    CSessionManager();
    
    /**
     * @brief 析构函数
     */
    ~CSessionManager();
    
    /**
     * @brief 创建会话
     * @param consoleId 操作台ID
     * @param consoleType 操作台类型
     * @return std::string 会话ID
     */
    std::string createSession(const std::string& consoleId, cmd::ECommandSource consoleType);
    
    /**
     * @brief 关闭会话
     * @param sessionId 会话ID
     * @return bool 关闭是否成功
     */
    bool closeSession(const std::string& sessionId);
    
    /**
     * @brief 绑定设备到会话
     * @param sessionId 会话ID
     * @param deviceId 设备ID
     * @return bool 绑定是否成功
     */
    bool bindDevice(const std::string& sessionId, const std::string& deviceId);
    
    /**
     * @brief 从会话解绑设备
     * @param sessionId 会话ID
     * @param deviceId 设备ID
     * @return bool 解绑是否成功
     */
    bool unbindDevice(const std::string& sessionId, const std::string& deviceId);
    
    /**
     * @brief 获取会话绑定的所有设备
     * @param sessionId 会话ID
     * @param[out] devices 设备列表
     * @return bool 获取是否成功
     */
    bool getBoundDevices(const std::string& sessionId, std::vector<std::string>& devices);
    
    /**
     * @brief 处理指令
     * @param rawData 原始指令数据
     * @param sourceType 指令来源类型
     * @param sessionId 会话ID
     * @return CommandResult 指令执行结果
     */
    CommandResult processCommand(const std::string& rawData, const std::string& sourceType, const std::string& sessionId);
    
    /**
     * @brief 处理指令（重载）
     * @param cmd 系统指令对象
     * @param sessionId 会话ID
     * @return CommandResult 指令执行结果
     */
    CommandResult processCommand(const cmd::CSystemCmd& cmd, const std::string& sessionId);
    
    /**
     * @brief 刷新会话活跃时间
     * @param sessionId 会话ID
     * @return bool 刷新是否成功
     */
    bool refreshSession(const std::string& sessionId);
    
    /**
     * @brief 检查会话是否有效
     * @param sessionId 会话ID
     * @return bool 会话是否有效
     */
    bool isValidSession(const std::string& sessionId);
    
    /**
     * @brief 获取会话信息
     * @param sessionId 会话ID
     * @param[out] info 会话信息
     * @return bool 获取是否成功
     */
    bool getSessionInfo(const std::string& sessionId, SessionInfo& info);
    
    /**
     * @brief 设置会话超时时间
     * @param timeoutMs 超时时间（毫秒）
     */
    void setSessionTimeout(uint32_t timeoutMs);
    
    /**
     * @brief 清理超时会话
     * @return uint32_t 清理的会话数量
     */
    uint32_t cleanTimeoutSessions();
    
    /**
     * @brief 注册指令执行回调函数
     * @param cmdType 指令类型
     * @param callback 回调函数
     * @return bool 注册是否成功
     */
    bool registerCommandCallback(cmd::ECommandType cmdType, 
        std::function<CommandResult(const cmd::CSystemCmd&, const std::string& sessionId)> callback);
    
    /**
     * @brief 获取所有活跃会话
     * @return std::vector<SessionInfo> 活跃会话列表
     */
    std::vector<SessionInfo> getAllActiveSessions();
    
private:
    /**
     * @brief 生成唯一会话ID
     * @return std::string 唯一会话ID
     */
    std::string generateSessionId();
    
    /**
     * @brief 检查会话是否超时
     * @param sessionInfo 会话信息
     * @return bool 是否超时
     */
    bool isSessionTimeout(const SessionInfo& sessionInfo) const;
    
    /**
     * @brief 执行指令
     * @param cmd 系统指令对象
     * @param sessionId 会话ID
     * @return CommandResult 指令执行结果
     */
    CommandResult executeCommand(const cmd::CSystemCmd& cmd, const std::string& sessionId);
    
    /**
     * @brief 验证会话权限
     * @param sessionId 会话ID
     * @param cmd 系统指令对象
     * @return bool 是否有权限执行指令
     */
    bool validateSessionPermission(const std::string& sessionId, const cmd::CSystemCmd& cmd);
    
    /**
     * @brief 会话映射表，会话ID到会话信息的映射
     */
    std::unordered_map<std::string, SessionInfo> m_sessionMap;
    
    /**
     * @brief 操作台ID到会话ID的映射，用于快速查找操作台对应的会话
     */
    std::unordered_map<std::string, std::string> m_consoleSessionMap;
    
    /**
     * @brief 指令执行回调函数映射表
     */
    std::unordered_map<cmd::ECommandType, 
        std::function<CommandResult(const cmd::CSystemCmd&, const std::string&)>> m_commandCallbacks;
    
    /**
     * @brief 互斥锁，保证线程安全
     */
    mutable std::mutex m_mutex;
    
    /**
     * @brief 会话超时时间（毫秒）
     */
    uint32_t m_sessionTimeoutMs;
    
    /**
     * @brief 解析器工厂实例
     */
    cmd::CConsoleCmdParserFactory& m_parserFactory;
};

} // namespace session
} // namespace business
