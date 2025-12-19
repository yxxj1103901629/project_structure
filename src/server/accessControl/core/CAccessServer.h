#ifndef CACCESSSERVER_H
#define CACCESSSERVER_H

#include <string>
#include <atomic>
#include <thread>
#include <mutex>

// 前向声明
class CAccessManager;
/**
 * @brief 门禁服务器类
 *
 * 负责监听门禁终端的连接请求，处理终端数据，并与门禁管理器进行通信。
 */
class CAccessServer
{
public:
    /**
     * @brief 构造函数
     * @param port 服务器监听端口
     */
    CAccessServer(int port = 8080);
    
    /**
     * @brief 析构函数
     */
    ~CAccessServer();

    /**
     * @brief 启动门禁服务器
     * @return 启动成功返回true，否则返回false
     * @note 启动服务器监听线程，开始接收门禁终端连接请求
     */
    bool start();

    /**
     * @brief 停止门禁服务器
     * @note 停止服务器监听线程，关闭所有客户端连接
     */
    void stop();

    /**
     * @brief 检查服务器是否正在运行
     * @return 运行中返回true，否则返回false
     */
    bool isRunning() const;

    /**
     * @brief 设置门禁管理器实例
     * @param manager 门禁管理器指针
     * @note 用于服务器与管理器之间的通信
     */
    void setAccessManager(CAccessManager* manager);

private:
    /**
     * @brief 服务器监听线程函数
     * @note 监听客户端连接请求，接收连接后创建处理线程
     */
    void listenThreadFunc();

    /**
     * @brief 处理客户端连接
     * @param clientSocket 客户端套接字
     * @note 接收客户端数据，解析终端ID，调用管理器处理终端连接
     */
    void handleClientConnection(int clientSocket);

    /**
     * @brief 处理客户端数据
     * @param clientSocket 客户端套接字
     * @param terminalId 终端ID
     * @note 持续接收客户端数据，调用管理器处理终端数据
     */
    void handleClientData(int clientSocket, const std::string& terminalId);

    int m_port;                          ///< 服务器监听端口
    std::atomic<bool> m_isRunning;       ///< 服务器运行状态
    std::thread m_listenThread;          ///< 监听线程
    
    CAccessManager* m_accessManager;     ///< 门禁管理器指针
    mutable std::mutex m_managerMutex;   ///< 管理器指针互斥锁
};

#endif // CACCESSSERVER_H
