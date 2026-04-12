#pragma once

#include "NetDefs.h"
#include <memory>

#ifdef NETWORK_EXPORTS
#define CTCP_CLIENT_API __declspec(dllexport)
#else
#define CTCP_CLIENT_API __declspec(dllimport)
#endif

namespace asio {

/**
 * @brief TCP 客户端门面（Facade + Pimpl）。
 * @details 通过 Pimpl 隐藏所有网络细节；通过 Facade 提供简洁接口。
 */
class CTCP_CLIENT_API CTcpClient
{
public:
    CTcpClient();
    ~CTcpClient();
    CTcpClient(const CTcpClient&) = delete;
    CTcpClient& operator=(const CTcpClient&) = delete;

public:
    /**
     * @brief 初始化 TCP 客户端（启动 IO / Task 线程池）。
     * @return 成功返回 @c true；重复初始化或资源不足返回 @c false。
     */
    bool init() noexcept;

    /**
     * @brief 发起到服务端的异步连接。
     * @param[in] serverAddr 服务端地址（IP + Port）。
     * @return 任务投递成功返回 @c true；未初始化返回 @c false。
     */
    bool connect(const NetAddr& serverAddr) noexcept;

    /**
     * @brief 主动断开连接并取消自动重连。
     */
    void disconnect() noexcept;

    /**
     * @brief 异步发送数据。
     * @param[in] data 待发送数据视图。
     * @return 成功入队返回 @c true；未连接、未初始化或数据为空返回 @c false。
     */
    bool send(std::string_view data) noexcept;
    bool send(const std::string& data) noexcept;
    bool send(const char* data, size_t length) noexcept;

    /**
     * @brief 设置事件回调（移动语义）。
     * @details 推荐使用 `ClientCallback::Builder{}...build()` 构造回调对象。
     */
    void setCallback(ClientCallback&& callback) noexcept;
    /**
     * @brief 设置事件回调（拷贝语义）。
     */
    void setCallback(const ClientCallback& callback) noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> m_pImpl;
};

} // namespace asio
