// =========================================================================
// Module: session
// File: CConsoleCmdParserFactory.h
// Version: 1.0.0
// Description: 操作台指令解析器工厂实现
// Author: Session Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "../base/IConsoleCmdParser.h"

namespace business {
namespace session {
namespace cmd {

/**
 * @class CConsoleCmdParserFactory
 * @brief 操作台指令解析器工厂类
 * @details 实现解析器对象的动态创建机制，支持解析器的注册、获取和生命周期管理
 */
class CConsoleCmdParserFactory {
public:
    /**
     * @brief 获取工厂实例（单例模式）
     * @return CConsoleCmdParserFactory& 工厂实例引用
     */
    static CConsoleCmdParserFactory& getInstance();
    
    /**
     * @brief 注册解析器方法
     * @param sourceType 指令来源类型
     * @param creator 解析器创建函数
     * @return bool 注册是否成功
     */
    bool registerParser(const std::string& sourceType, std::function<std::shared_ptr<IConsoleCmdParser>()> creator);
    
    /**
     * @brief 注册解析器方法（重载）
     * @param source 指令来源枚举
     * @param creator 解析器创建函数
     * @return bool 注册是否成功
     */
    bool registerParser(ECommandSource source, std::function<std::shared_ptr<IConsoleCmdParser>()> creator);
    
    /**
     * @brief 获取解析器方法
     * @param sourceType 指令来源类型
     * @return std::shared_ptr<IConsoleCmdParser> 解析器实例
     */
    std::shared_ptr<IConsoleCmdParser> getParser(const std::string& sourceType);
    
    /**
     * @brief 获取解析器方法（重载）
     * @param source 指令来源枚举
     * @return std::shared_ptr<IConsoleCmdParser> 解析器实例
     */
    std::shared_ptr<IConsoleCmdParser> getParser(ECommandSource source);
    
    /**
     * @brief 获取所有已注册的解析器类型
     * @return std::vector<std::string> 已注册的解析器类型列表
     */
    std::vector<std::string> getAllRegisteredParserTypes();
    
    /**
     * @brief 注销解析器方法
     * @param sourceType 指令来源类型
     * @return bool 注销是否成功
     */
    bool unregisterParser(const std::string& sourceType);
    
    /**
     * @brief 清空所有已注册的解析器
     */
    void clearAllParsers();
    
private:
    /**
     * @brief 构造函数（私有，单例模式）
     */
    CConsoleCmdParserFactory();
    
    /**
     * @brief 析构函数（私有，单例模式）
     */
    ~CConsoleCmdParserFactory() = default;
    
    /**
     * @brief 拷贝构造函数（禁用，单例模式）
     */
    CConsoleCmdParserFactory(const CConsoleCmdParserFactory&) = delete;
    
    /**
     * @brief 赋值运算符（禁用，单例模式）
     */
    CConsoleCmdParserFactory& operator=(const CConsoleCmdParserFactory&) = delete;
    
    /**
     * @brief 解析器创建函数映射表
     */
    std::unordered_map<std::string, std::function<std::shared_ptr<IConsoleCmdParser>()>> m_parserCreators;
    
    /**
     * @brief 解析器实例缓存
     */
    std::unordered_map<std::string, std::shared_ptr<IConsoleCmdParser>> m_parserCache;
    
    /**
     * @brief 互斥锁，保证线程安全
     */
    mutable std::mutex m_mutex;
};

} // namespace cmd
} // namespace session
} // namespace business
