// =========================================================================
// Module: session
// File: IConsoleCmdParser.h
// Version: 1.0.0
// Description: 操作台指令解析器基类接口定义
// Author: Session Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <string>
#include <vector>
#include "CSystemCmd.h"

namespace business {
namespace session {
namespace cmd {

/**
 * @class IConsoleCmdParser
 * @brief 操作台指令解析器基类接口
 * @details 定义所有指令解析器派生类必须实现的统一接口规范
 */
class IConsoleCmdParser {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~IConsoleCmdParser() = default;
    
    /**
     * @brief 指令解析方法
     * @param rawData 原始指令数据
     * @param[out] cmd 解析后的系统指令对象
     * @return bool 解析是否成功
     */
    virtual bool parseCommand(const std::string& rawData, CSystemCmd& cmd) = 0;
    
    /**
     * @brief 指令验证方法
     * @param cmd 待验证的系统指令对象
     * @return bool 指令是否合法
     */
    virtual bool validateCommand(const CSystemCmd& cmd) = 0;
    
    /**
     * @brief 指令类型识别方法
     * @return std::vector<ECommandType> 当前解析器支持的指令类型列表
     */
    virtual std::vector<ECommandType> getSupportedCommandTypes() = 0;
    
    /**
     * @brief 获取解析器名称
     * @return std::string 解析器名称
     */
    virtual std::string getParserName() const = 0;
    
    /**
     * @brief 获取支持的指令来源
     * @return ECommandSource 支持的指令来源
     */
    virtual ECommandSource getSupportedSource() const = 0;
};

} // namespace cmd
} // namespace session
} // namespace business
