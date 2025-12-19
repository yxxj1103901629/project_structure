// =========================================================================
// Module: security
// File: CSecurityStrategyFactory.h
// Version: 1.0.0
// Description: 安全策略工厂类定义，提供策略创建、注册与缓存功能
// Author: Security Framework Team
// Date: 2024-01-01
// =========================================================================

#pragma once

#include "business/security/base/ISecurityStrategy.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace business {
namespace security {

/**
 * @class CSecurityStrategyFactory
 * @brief 安全策略工厂类，提供按异常类型创建策略实例的接口
 * @note 采用单例模式实现，确保全局唯一的工厂实例
 */
class CSecurityStrategyFactory
{
public:
    /**
     * @brief 获取工厂单例实例
     * @return CSecurityStrategyFactory& 工厂实例引用
     */
    static CSecurityStrategyFactory& getInstance();

    /**
     * @brief 销毁工厂单例实例（仅用于测试或特殊场景）
     */
    static void destroyInstance();

    /**
     * @brief 创建策略实例
     * @param exceptionType 异常类型
     * @return std::shared_ptr<ISecurityStrategy> 策略实例指针
     * @note 如果缓存可用且启用，将返回缓存的实例
     */
    std::shared_ptr<ISecurityStrategy> createStrategy(const std::string& exceptionType);

    /**
     * @brief 注册策略创建函数
     * @param exceptionType 异常类型
     * @param creatorFunc 策略创建函数
     * @return bool 注册是否成功
     * @note 支持覆盖已存在的策略注册
     */
    bool registerStrategy(const std::string& exceptionType,
                          std::function<std::shared_ptr<ISecurityStrategy>()> creatorFunc);

    /**
     * @brief 注销策略
     * @param exceptionType 异常类型
     * @return bool 注销是否成功
     */
    bool unregisterStrategy(const std::string& exceptionType);

    /**
     * @brief 清空所有策略注册
     */
    void clearAllStrategies();

    /**
     * @brief 获取已注册的策略类型列表
     * @return std::vector<std::string> 策略类型列表
     */
    std::vector<std::string> getRegisteredStrategyTypes() const;

    /**
     * @brief 检查策略是否已注册
     * @param exceptionType 异常类型
     * @return bool 是否已注册
     */
    bool isStrategyRegistered(const std::string& exceptionType) const;

    /**
     * @brief 设置策略实例缓存开关
     * @param enableCache 是否启用缓存
     */
    void setCacheEnabled(bool enableCache);

    /**
     * @brief 设置缓存最大大小
     * @param maxSize 缓存最大大小
     */
    void setCacheMaxSize(size_t maxSize);

    /**
     * @brief 清空策略实例缓存
     */
    void clearCache();

    /**
     * @brief 获取当前缓存大小
     * @return size_t 当前缓存大小
     */
    size_t getCacheSize() const;

private:
    /**
     * @brief 构造函数（私有，防止外部实例化）
     */
    CSecurityStrategyFactory();

    /**
     * @brief 析构函数（私有，防止外部删除）
     */
    ~CSecurityStrategyFactory();

    /**
     * @brief 从缓存获取策略实例
     * @param exceptionType 异常类型
     * @return std::shared_ptr<ISecurityStrategy> 策略实例指针， nullptr表示缓存未命中
     */
    std::shared_ptr<ISecurityStrategy> getFromCache(const std::string& exceptionType);

    /**
     * @brief 将策略实例添加到缓存
     * @param exceptionType 异常类型
     * @param strategy 策略实例指针
     */
    void addToCache(const std::string& exceptionType,
                    const std::shared_ptr<ISecurityStrategy>& strategy);

    // 单例实例指针
    static CSecurityStrategyFactory* m_instance;

    // 策略创建函数映射表
    std::unordered_map<std::string, std::function<std::shared_ptr<ISecurityStrategy>()>>
        m_strategyCreators;

    // 策略实例缓存
    std::unordered_map<std::string, std::shared_ptr<ISecurityStrategy>> m_strategyCache;

    // 缓存配置
    bool m_enableCache;    ///< 是否启用缓存
    size_t m_cacheMaxSize; ///< 缓存最大大小

    // 线程安全互斥锁
    mutable std::mutex m_mutex;
    mutable std::mutex m_cacheMutex;

    // 禁用拷贝构造和赋值操作
    CSecurityStrategyFactory(const CSecurityStrategyFactory&) = delete;
    CSecurityStrategyFactory& operator=(const CSecurityStrategyFactory&) = delete;
};

} // namespace security
} // namespace business
