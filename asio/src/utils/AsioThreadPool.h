#pragma once

#include "MakeHelper.h"

#include <boost/asio.hpp>
#include <thread>
#include <vector>

namespace asio {

using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
using WorkGuardPtr = std::unique_ptr<WorkGuard>;
using ThreadGroup = std::vector<std::thread>;

template<typename ErrorCb>
bool startAsioThreadPool(size_t n,
                         boost::asio::io_context& ctx,
                         WorkGuardPtr& wg,
                         ThreadGroup& pool,
                         ErrorCb&& onError) noexcept
{
    WorkGuardPtr localGuard = make_unique_noexcept<WorkGuard>(boost::asio::make_work_guard(ctx));
    if (!localGuard) {
        onError("work guard alloc failed");
        return false;
    }

    ThreadGroup localThreads;
    try {
        localThreads.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            localThreads.emplace_back([&ctx, onError] {
                try {
                    ctx.run();
                } catch (const std::exception& e) {
                    onError(e.what());
                } catch (...) {
                    onError("unknown thread exception");
                }
            });
        }
    } catch (const std::exception& e) {
        localGuard.reset();
        ctx.stop();
        for (auto& thread : localThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        ctx.restart();
        onError(e.what());
        return false;
    }

    wg = std::move(localGuard);
    pool = std::move(localThreads);
    return true;
}

} // namespace asio
