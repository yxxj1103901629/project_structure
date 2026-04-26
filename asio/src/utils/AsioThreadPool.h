#pragma once

#include "MakeHelper.h"

#include <boost/asio.hpp>
#include <thread>
#include <vector>

namespace asio {

using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
using WorkGuardPtr = std::unique_ptr<WorkGuard>;
using ThreadGroup = std::vector<std::thread>;

/**
 * @brief 统一启动 asio 线程池（消除 CTcpClientImpl/CTcpServerImpl 的重复代码）。
 *
 * 步骤：创建 work_guard → reserve 容量 → 批量 emplace 线程。
 * work_guard 防止 context 在所有任务完成前自动退出。
 *
 * @param n         线程数量。
 * @param ctx       目标 io_context。
 * @param wg        输出 work_guard。
 * @param pool      输出已启动线程集合。
 * @param onError   线程内异常时的错误回调。
 * @return 成功返回 true。
 */
template<typename ErrorCb>
bool startAsioThreadPool(size_t n,
                         boost::asio::io_context &ctx,
                         WorkGuardPtr &wg,
                         ThreadGroup &pool,
                         ErrorCb &&onError) noexcept
{
    wg = make_unique_noexcept<WorkGuard>(boost::asio::make_work_guard(ctx));
    if (!wg) {
        onError("work guard alloc failed");
        return false;
    }

    try {
        pool.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            pool.emplace_back([&ctx, onError] {
                try {
                    ctx.run();
                } catch (const std::exception &e) {
                    onError(e.what());
                } catch (...) {
                    onError("unknown thread exception");
                }
            });
        }
    } catch (const std::exception &e) {
        onError(e.what());
        return false;
    }
    return true;
}

} // namespace asio
