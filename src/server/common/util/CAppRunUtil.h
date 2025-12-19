#ifndef CAPPRUNUTIL_H
#define CAPPRUNUTIL_H

/**
 * @file CAppRunUtil.h
 * @brief 应用程序运行工具类头文件
 *
 * 提供错误处理、信号处理、控制台编码初始化和TBB任务管理等功能
 */

// 标准库头文件
#include <chrono>
#include <csignal>
#include <exception>
#include <string>
#include <thread>

// 系统特定头文件
#ifdef _WIN32
// 定义此宏以防止windows.h自动包含旧版本的winsock.h
#define _WINSOCKAPI_
#include <winsock2.h>
#include <windows.h>

#else
#include <unistd.h>
#endif

// oneAPI TBB 头文件
#include <oneapi/tbb/task_group.h>

/**
 * @class ErrorHandler
 * @brief 错误处理工具类
 */
class ErrorHandler
{
public:
    /**
     * @brief 显示错误消息
     * @param title 错误标题
     * @param message 错误消息内容
     */
    static void showErrorMessage(const char* title, const char* message)
    {
#ifdef _WIN32
        MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
#else
        std::cerr << "[ERROR] " << title << ": " << message << std::endl;
#endif
    }

    /**
     * @brief 显示错误消息（重载，支持std::string）
     * @param title 错误标题
     * @param message 错误消息内容
     */
    static void showErrorMessage(const std::string& title, const std::string& message)
    {
        showErrorMessage(title.c_str(), message.c_str());
    }
};

/**
 * @class SignalHandler
 * @brief 信号处理封装类 - 单例模式
 * 
 * 处理应用程序终止信号（SIGINT、SIGTERM等），使用条件变量实现高效的信号等待
 */
class SignalHandler
{
public:
    /**
     * @brief 获取单例实例
     * @return SignalHandler& 单例实例引用
     */
    static SignalHandler& getInstance()
    {
        static SignalHandler instance;
        return instance;
    }

    /**
     * @brief 等待终止信号
     * 
     * 使用定期检查原子标志的方式，确保信号处理的安全性
     */
    void waitForTermination()
    {
        while (!shouldTerminate()) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);
#endif
        }
    }

    /**
     * @brief 检查是否收到终止信号
     * @return bool 如果收到终止信号则返回true，否则返回false
     */
    bool shouldTerminate() const { return terminate_; }

private:
    /**
     * @brief 构造函数 - 私有，防止外部创建实例
     * 
     * 注册信号处理函数
     */
    SignalHandler()
        : terminate_(false)
    {
        // 注册信号处理函数
        signal(SIGINT, &SignalHandler::handleSignal);
        signal(SIGTERM, &SignalHandler::handleSignal);
    }

    ~SignalHandler() = default;

    // 删除拷贝构造函数和赋值运算符，确保单例
    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

    /**
     * @brief 信号处理函数
     * @param signal 收到的信号
     * 
     * 注意：信号处理函数应尽可能简单，避免使用可能不安全的操作
     */
    static void handleSignal(int signal)
    {
        if (signal == SIGINT || signal == SIGTERM) {
            // 在信号处理函数中只设置原子标志，避免复杂操作
            getInstance().terminate_ = true;
        }
    }

    volatile sig_atomic_t terminate_; ///< 终止标志 - 原子变量，确保信号安全
};

/**
 * @class ConsoleUtils
 * @brief 控制台工具类
 */
class ConsoleUtils
{
public:
    /**
     * @brief 初始化控制台编码
     * 
     * 在Windows平台上设置UTF-8编码，确保中文等非ASCII字符正常显示
     */
    static void initConsoleEncoding()
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }
};

/**
 * @class TaskManager
 * @brief TBB任务管理器 - 单例模式
 * 
 * 封装TBB任务组，提供简单的任务提交接口
 */
class TaskManager
{
public:
    /**
     * @brief 获取单例实例
     * @return TaskManager& 单例实例引用
     */
    static TaskManager& getInstance()
    {
        static TaskManager instance;
        return instance;
    }

    /**
     * @brief 提交单个任务
     * @tparam Func 任务函数类型
     * @param func 任务函数
     */
    template<typename Func>
    static void addTask(Func&& func)
    {
        getInstance().task_group_.run(std::forward<Func>(func));
    }

    /**
     * @brief 提交多个任务
     * @tparam Funcs 任务函数类型包
     * @param funcs 任务函数包
     */
    template<typename... Funcs>
    static void addTasks(Funcs&&... funcs)
    {
        auto& instance = getInstance();
        (instance.task_group_.run(std::forward<Funcs>(funcs)), ...);
    }

    /**
     * @brief 等待所有任务完成
     */
    static void waitForAllTasks() { getInstance().task_group_.wait(); }

private:
    TaskManager() { ConsoleUtils::initConsoleEncoding(); }
    ~TaskManager()
    {
        task_group_.wait(); // 确保所有任务完成
    }

    // 删除拷贝构造函数和赋值运算符，确保单例
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    tbb::task_group task_group_; ///< TBB任务组
};

/**
 * @class AppRunner
 * @brief 应用程序运行器类
 * 
 * 提供应用程序初始化、任务管理和优雅退出功能
 */
class AppRunner
{
public:
    /**
     * @brief 打包应用程序任务
     * @tparam InitFunc 初始化函数类型
     * @tparam CleanupFunc 清理函数类型
     * @param init 初始化函数
     * @param cleanup 清理函数
     * @return 封装后的任务函数
     */
    template<typename InitFunc, typename CleanupFunc>
    static auto packageAppTask(InitFunc&& init, CleanupFunc&& cleanup)
    {
        return [init = std::forward<InitFunc>(init), cleanup = std::forward<CleanupFunc>(cleanup)]() {
                try {
                    // 执行初始化代码块
                    init();

                    // 等待终止信号
                    SignalHandler::getInstance().waitForTermination();

                    // 执行清理代码块
                    cleanup();

                    // 延迟退出，确保清理完成
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } catch (const std::exception& ex) {
                    ErrorHandler::showErrorMessage("Task Exception", ex.what());
                } catch (...) {
                    ErrorHandler::showErrorMessage("Task Exception",
                                                   "An unknown error occurred in the task.");
                }
            };
    }
};

#endif // CAPPRUNUTIL_H
