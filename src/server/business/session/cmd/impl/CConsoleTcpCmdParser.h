// =========================================================================
// Module: session
// File: CConsoleTcpCmdParser.h
// Version: 1.0.0
// Description: TCP操作台指令解析器实现
// Author: Session Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include "../base/IConsoleCmdParser.h"

namespace business {
namespace session {
namespace cmd {

/**
 * @class CConsoleTcpCmdParser
 * @brief TCP操作台指令解析器派生类
 * @details 实现TCP协议指令的解析逻辑，支持基于文本的TCP指令格式
 */
class CConsoleTcpCmdParser : public IConsoleCmdParser {
public:
    /**
     * @brief 构造函数
     */
    CConsoleTcpCmdParser();
    
    /**
     * @brief 析构函数
     */
    ~CConsoleTcpCmdParser() override;
    
    /**
     * @brief 指令解析方法
     * @param rawData 原始TCP指令数据
     * @param[out] cmd 解析后的系统指令对象
     * @return bool 解析是否成功
     */
    bool parseCommand(const std::string& rawData, CSystemCmd& cmd) override;
    
    /**
     * @brief 指令验证方法
     * @param cmd 待验证的系统指令对象
     * @return bool 指令是否合法
     */
    bool validateCommand(const CSystemCmd& cmd) override;
    
    /**
     * @brief 指令类型识别方法
     * @return std::vector<ECommandType> 当前解析器支持的指令类型列表
     */
    std::vector<ECommandType> getSupportedCommandTypes() override;
    
    /**
     * @brief 获取解析器名称
     * @return std::string 解析器名称
     */
    std::string getParserName() const override;
    
    /**
     * @brief 获取支持的指令来源
     * @return ECommandSource 支持的指令来源
     */
    ECommandSource getSupportedSource() const override;
    
private:
    /**
     * @brief 解析TCP指令头部
     * @param rawData 原始TCP指令数据
     * @param[out] cmdType 指令类型
     * @param[out] cmdId 指令ID
     * @return bool 解析是否成功
     */
    bool parseTcpHeader(const std::string& rawData, ECommandType& cmdType, std::string& cmdId);
    
    /**
     * @brief 解析TCP指令参数
     * @param rawData 原始TCP指令数据
     * @param[out] params 参数映射表
     * @return bool 解析是否成功
     */
    bool parseTcpParameters(const std::string& rawData, std::unordered_map<std::string, std::string>& params);
    
    /**
     * @brief 验证TCP指令格式
     * @param rawData 原始TCP指令数据
     * @return bool 格式是否合法
     */
    bool validateTcpFormat(const std::string& rawData);
    
    /**
     * @brief 解析器支持的指令类型列表
     */
    std::vector<ECommandType> m_supportedTypes;
};

} // namespace cmd
} // namespace session
} // namespace business
