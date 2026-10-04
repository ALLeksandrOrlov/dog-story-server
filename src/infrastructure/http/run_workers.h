#pragma once
#include <algorithm>
#include <boost/asio/io_context.hpp>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace http_server {
inline void RunWorkers(boost::asio::io_context& context, unsigned count) {
    std::exception_ptr failure;
    std::mutex mutex;
    auto run = [&] {
        try {
            context.run();
        } catch (...) {
            {
                std::lock_guard lock(mutex);
                if (!failure)
                    failure = std::current_exception();
            }
            context.stop();
        }
    };
    {
        std::vector<std::jthread> workers;
        count = std::max(1u, count);
        workers.reserve(count - 1);
        try {
            for (unsigned i = 1; i < count; ++i)
                workers.emplace_back(run);
        } catch (...) {
            context.stop();
            throw;
        }
        run();
    }
    if (failure)
        std::rethrow_exception(failure);
}
} // namespace http_server
