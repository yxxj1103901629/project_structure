#ifndef CUVMULTILOOP_H
#define CUVMULTILOOP_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <uv.h>
#include <vector>

#include "concurrentqueue.h"

namespace OutSide {
namespace Common {
namespace EventLoop {

/**
 * @brief 多事件循环类，封装了多个libuv事件循环的功能
 * @details 采用单例模式，自动管理多个工作线程的启动和停止
 */
class CUVMultiloop
{
private:
    /**
     * @brief LoopWorker结构体，封装单个事件循环的线程和相关资源
     */
    struct LoopWorker
    {
        CUVMultiloop* manager = nullptr; // 指向CUVLoop实例的指针
        uv_loop_t* loop = nullptr;
        std::thread* thread = nullptr;
        std::atomic<bool> initialized = false;
        std::atomic<bool> stopping = false;
        uv_async_t asyncWork;
        uv_async_t asyncExit;
        moodycamel::ConcurrentQueue<std::function<void()>> taskQueue;
    };

private:
    /**
     * @brief 内部工作线程函数
     * @param arg 指向LoopWorker实例的指针
     */
    static void workerThread(void* arg);

private:
    // 私有构造函数，防止直接实例化
    CUVMultiloop();
    ~CUVMultiloop();

    // 禁止拷贝构造和赋值操作
    CUVMultiloop(const CUVMultiloop&) = delete;
    CUVMultiloop& operator=(const CUVMultiloop&) = delete;

public:
    /**
     * @brief 获取CUVLoop单例实例
     * @return CUVLoop实例指针
     */
    static CUVMultiloop* getInstance();

    /**
     * @brief 检查事件循环是否正在运行
     * @return 是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 获取事件循环线程数量
     * @return 事件循环线程数量
     */
    size_t getLoopCount() const;

    /**
     * @brief 向事件循环中提交一个任务
     * @param task 任务回调函数
     */
    void postTask(const std::function<void()>& task);

    /**
     * @brief 向事件循环中提交一个任务（右值引用版本）
     * @param task 任务回调函数
     */
    void postTask(std::function<void()>&& task);

private:
    /**
     * @brief 选择一个事件循环进行任务投递
     * @return 选中的LoopWorker指针
     */
    LoopWorker* selectLoopWorker();

private:
    std::vector<LoopWorker*> m_loopWorkers;
    std::atomic<bool> m_isStopping;
    std::atomic<size_t> m_nextWorkerIndex;
    size_t m_cpuCoreCount;

    // 线程同步机制
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_initialized;
};

} // namespace EventLoop
} // namespace Common
} // namespace OutSide

#endif // CUVLOOP_H
