#ifndef CUVONELOOP_H
#define CUVONELOOP_H

#include "concurrentqueue.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <uv.h>

namespace OutSide {
namespace Common {
namespace EventLoop {

/**
 * @brief 单事件循环类，封装了libuv事件循环的功能
 * @details 采用单例模式，自动管理工作线程的启动和停止
 */
class CUVOneloop
{
private:
    /**
     * @brief 内部工作线程函数
     * @param arg 指向CUVLoop实例的指针
     */
    static void workerThread(void *arg);

private:
    // 私有构造函数，防止直接实例化
    CUVOneloop();
    ~CUVOneloop();

    // 禁止拷贝构造和赋值操作
    CUVOneloop(const CUVOneloop &) = delete;
    CUVOneloop &operator=(const CUVOneloop &) = delete;

public:
    /**
     * @brief 获取CUVLoop单例实例
     * @return CUVLoop实例指针
     */
    static CUVOneloop *getInstance();

    /**
     * @brief 检查事件循环是否正在运行
     * @return 是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 获取libuv事件循环指针
     * @return uv_loop_t指针
     */
    uv_loop_t *getLoop() const;

    /**
     * @brief 向事件循环中提交一个任务
     * @param task 任务回调函数
     */
    void postTask(const std::function<void()> &task);

    /**
     * @brief 向事件循环中提交一个任务（右值引用版本）
     * @param task 任务回调函数
     */
    void postTask(std::function<void()> &&task);

private:
    uv_loop_t *m_loop;              // libuv事件循环指针
    std::thread *m_workerThread;    // 工作线程指针
    std::atomic<bool> m_isStopping; // 停止标志

    // 线程同步机制
    std::condition_variable m_condition;
    std::mutex m_mutex;
    bool m_loopInitialized;

    // 异步通信句柄
    uv_async_t m_asyncWork; // 异步任务触发句柄
    uv_async_t m_asyncExit; // 异步退出句柄

    // 任务队列（用于处理异步任务）
    moodycamel::ConcurrentQueue<std::function<void()>> m_taskQueue;
};

} // namespace EventLoop
} // namespace Common
} // namespace OutSide

#endif // CUVLOOPMANAGER_H
