#pragma once

#include <atomic>

/**
 * @class CAtomicUtil
 * @brief 原子操作工具类，提供一致的内存序语义。
 *
 * @details 该类封装了常用的原子操作，确保在多线程环境下具有正确的内存可见性和同步行为。
 *          load 使用 acquire 语义，store 使用 release 语义，exchange 和 compareExchange 使用 acq_rel 语义。
 */
class CAtomicUtil final
{
public:
    /**
    * @brief 原子操作工具函数，提供一致的内存序语义。
     * @tparam T 原子变量的类型。
     * @param atomicVar 原子变量引用。
     * @return T 原子变量的值。
     * @details load 使用 acquire 语义，store 使用 release 语义，exchange 和 compareExchange 使用 acq_rel 语义。
     */
    template<typename T>
    static T load(const std::atomic<T> &atomicVar) noexcept
    {
        return atomicVar.load(std::memory_order_acquire);
    }

    /**
     * @brief 原子存储操作，使用 release 语义。
     * @tparam T 原子变量的类型。
     * @param atomicVar 原子变量引用。
     * @param value 要存储的值。
     * @details load 使用 acquire 语义，store 使用 release 语义，exchange 和 compareExchange 使用 acq_rel 语义。
     */
    template<typename T>
    static void store(std::atomic<T> &atomicVar, typename std::atomic<T>::value_type value) noexcept
    {
        atomicVar.store(value, std::memory_order_release);
    }

    /**
     * @brief 原子交换操作，使用 acq_rel 语义。
     * @tparam T 原子变量的类型。
     * @param atomicVar 原子变量引用。
     * @param desired 要交换的新值。
     * @return T 原子变量的旧值。
     * @details load 使用 acquire 语义，store 使用 release 语义，exchange 和 compareExchange 使用 acq_rel 语义。
     */
    template<typename T>
    static T exchange(std::atomic<T> &atomicVar,
                      typename std::atomic<T>::value_type desired) noexcept
    {
        return atomicVar.exchange(desired, std::memory_order_acq_rel);
    }

    /**
     * @brief 原子比较并交换操作，使用 acq_rel 语义。
     * @tparam T 原子变量的类型。
     * @param atomicVar 原子变量引用。
     * @param expected 期望值引用。
     * @param desired 要交换的新值。
     * @return bool 是否成功交换。
     * @details load 使用 acquire 语义，store 使用 release 语义，exchange 和 compareExchange 使用 acq_rel 语义。
     */
    template<typename T>
    static bool compareExchange(std::atomic<T> &atomicVar, T &expected, T desired) noexcept
    {
        return atomicVar.compare_exchange_strong(expected,
                                                 desired,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
    }

    /**
     * @brief 原子加法操作。
     * @tparam T 原子变量的类型。
     * @param atomicVar 原子变量引用。
     * @param value 递增值。
     * @param order 内存序，默认 relaxed。
     * @return T 加法前的旧值。
     */
    template<typename T>
    static T fetchAdd(std::atomic<T> &atomicVar,
                      typename std::atomic<T>::value_type value,
                      std::memory_order order = std::memory_order_relaxed) noexcept
    {
        return atomicVar.fetch_add(value, order);
    }

private:
    CAtomicUtil() = default;
    ~CAtomicUtil() = default;
    CAtomicUtil(const CAtomicUtil &) = delete;
    CAtomicUtil &operator=(const CAtomicUtil &) = delete;
    CAtomicUtil(CAtomicUtil &&) = delete;
    CAtomicUtil &operator=(CAtomicUtil &&) = delete;
};