#pragma once

#include <memory>

/**
 * @brief 安全的std::make_unique封装，捕获异常并返回nullptr
 * @tparam T 要创建的对象类型
 * @tparam Args 构造函数参数类型列表
 * @param args 构造函数参数
 * @return 
 * - 成功时返回指向新对象的std::unique_ptr
 * - 失败时（如内存分配失败）返回nullptr
 * @note 
 * - 适用于不希望抛出异常的场景
 * - 使用时需检查返回值是否为nullptr以避免解引用空指针
 */
template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique_noexcept(Args &&...args) noexcept
{
    try {
        return std::make_unique<T>(std::forward<Args>(args)...);
    } catch (...) {
        return nullptr;
    }
}

/**
 * @brief 安全的std::make_shared封装，捕获异常并返回nullptr
 * @tparam T 要创建的对象类型
 * @tparam Args 构造函数参数类型列表
 * @param args 构造函数参数
 * @return 
 * - 成功时返回指向新对象的std::shared_ptr
 * - 失败时（如内存分配失败）返回nullptr
 * @note 
 * - 适用于不希望抛出异常的场景
 * - 使用时需检查返回值是否为nullptr以避免解引用空指针
 */
template<typename T, typename... Args>
inline std::shared_ptr<T> make_shared_noexcept(Args &&...args) noexcept
{
    try {
        return std::make_shared<T>(std::forward<Args>(args)...);
    } catch (...) {
        return nullptr;
    }
}