#include "CUVMultiloop.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <uv.h>
#include <vector>

#include "concurrentqueue.h"

using namespace OutSide::Common::EventLoop;

// Task类型定义
using Task = std::function<void()>;

// 私有构造函数
CUVMultiloop::CUVMultiloop()
    : m_isStopping(false)
    , m_nextWorkerIndex(0)
    , m_initialized(false)
{
    // 获取系统CPU核心数
    m_cpuCoreCount = std::thread::hardware_concurrency();
    if (m_cpuCoreCount == 0) {
        // 如果无法获取，默认使用4个核心
        m_cpuCoreCount = 4;
    }

    // 创建建议数量的事件循环线程（CPU核心数 * 2）
    size_t workerCount = m_cpuCoreCount * 2;
    m_loopWorkers.reserve(workerCount);

    // 创建所有LoopWorker实例
    for (size_t i = 0; i < workerCount; ++i) {
        LoopWorker* worker = new LoopWorker();
        worker->manager = this; // 设置CUVLoop实例指针
        m_loopWorkers.push_back(worker);
        // 启动工作线程
        worker->thread = new std::thread(workerThread, worker);
    }

    // 等待所有工作线程初始化完成
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this]() {
        for (const auto& worker : m_loopWorkers) {
            if (!worker->initialized) {
                return false;
            }
        }
        return true;
    });
    // 设置整个CUVLoop已经初始化完成
    m_initialized = true;
}

// 析构函数
CUVMultiloop::~CUVMultiloop()
{
    m_isStopping = true;

    // 通知所有工作线程停止
    for (auto& worker : m_loopWorkers) {
        if (worker->loop && !worker->stopping) {
            worker->stopping = true;
            uv_async_send(&worker->asyncExit);
        }
    }

    // 等待所有工作线程完成
    for (auto& worker : m_loopWorkers) {
        if (worker->thread != nullptr && worker->thread->joinable()) {
            worker->thread->join();
            delete worker->thread;
            worker->thread = nullptr;
        }
        delete worker;
    }

    // 清理工作线程容器
    m_loopWorkers.clear();
}

// 内部工作线程函数
void CUVMultiloop::workerThread(void* arg)
{
    LoopWorker* worker = static_cast<LoopWorker*>(arg);

    // 初始化libuv循环
    worker->loop = new uv_loop_t;
    if (uv_loop_init(worker->loop) != 0) {
        delete worker->loop;
        worker->loop = nullptr;

        // 标记worker初始化失败
        worker->initialized = true;
        // 通知主线程条件变量
        {
            std::unique_lock<std::mutex> lock(worker->manager->m_mutex);
            worker->manager->m_condition.notify_one();
        }

        return;
    }

    // 初始化异步退出句柄
    worker->asyncExit.data = worker;
    int exitInitResult = uv_async_init(worker->loop, &worker->asyncExit, [](uv_async_t* handle) {
        LoopWorker* worker = static_cast<LoopWorker*>(handle->data);

        // 清理所有未处理的任务
        Task task;
        while (worker->taskQueue.try_dequeue(task)) {
            // 任务对象会被自动销毁
        }

        // 停止libuv循环
        uv_stop(handle->loop);
    });

    // 初始化异步任务处理句柄
    worker->asyncWork.data = worker;
    int workInitResult = uv_async_init(worker->loop, &worker->asyncWork, [](uv_async_t* handle) {
        LoopWorker* worker = static_cast<LoopWorker*>(handle->data);

        Task task;
        while (worker->taskQueue.try_dequeue(task)) {
            if (task) {
                task();
            }
        }
    });

    // 初始化结果检查
    if (exitInitResult != 0 || workInitResult != 0) {
        delete worker->loop;
        worker->loop = nullptr;
        worker->initialized = true;
        // 通知主线程条件变量
        {
            std::unique_lock<std::mutex> lock(worker->manager->m_mutex);
            worker->manager->m_condition.notify_one();
        }
        return;
    }

    // 标记worker已经初始化完成
    worker->initialized = true;
    // 通知主线程条件变量
    {
        std::unique_lock<std::mutex> lock(worker->manager->m_mutex);
        worker->manager->m_condition.notify_one();
    }

    // 运行libuv循环
    uv_run(worker->loop, UV_RUN_DEFAULT);

    // 关闭所有uv句柄
    uv_close(reinterpret_cast<uv_handle_t*>(&worker->asyncExit), [](uv_handle_t*) {});
    uv_close(reinterpret_cast<uv_handle_t*>(&worker->asyncWork), [](uv_handle_t*) {});

    // 处理所有剩余的事件，直到loop不再活跃
    while (uv_loop_alive(worker->loop)) {
        uv_run(worker->loop, UV_RUN_DEFAULT);
    }

    // 关闭并清理loop
    uv_loop_close(worker->loop);
    delete worker->loop;
    worker->loop = nullptr;
}

// 获取单例实例
CUVMultiloop* CUVMultiloop::getInstance()
{
    static CUVMultiloop instance;
    return &instance;
}

// 检查事件循环是否正在运行
bool CUVMultiloop::isRunning() const
{
    return !m_isStopping && m_initialized;
}

// 获取事件循环线程数量
size_t CUVMultiloop::getLoopCount() const
{
    return m_loopWorkers.size();
}

// 负载均衡算法：轮询选择一个事件循环
CUVMultiloop::LoopWorker* CUVMultiloop::selectLoopWorker()
{
    if (m_loopWorkers.empty()) {
        return nullptr;
    }

    // 轮询选择下一个worker
    size_t index = m_nextWorkerIndex.fetch_add(1, std::memory_order_relaxed) % m_loopWorkers.size();
    return m_loopWorkers[index];
}

// 向循环中提交任务（左值引用版本）
void CUVMultiloop::postTask(const std::function<void()>& task)
{
    if (!m_initialized || m_isStopping) {
        return;
    }

    // 选择一个事件循环进行任务投递
    LoopWorker* worker = selectLoopWorker();
    if (worker && worker->loop && !worker->stopping) {
        worker->taskQueue.enqueue(task);
        // 触发异步任务处理
        uv_async_send(&worker->asyncWork);
    }
}

// 向循环中提交任务（右值引用版本）
void CUVMultiloop::postTask(std::function<void()>&& task)
{
    if (!m_initialized || m_isStopping) {
        return;
    }

    // 选择一个事件循环进行任务投递
    LoopWorker* worker = selectLoopWorker();
    if (worker && worker->loop && !worker->stopping) {
        worker->taskQueue.enqueue(std::move(task));
        // 触发异步任务处理
        uv_async_send(&worker->asyncWork);
    }
}
