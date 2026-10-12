#pragma once
#include <algorithm>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

namespace http_server
{
/**
 * @brief Выполняет context на указанном числе потоков, включая вызывающий.
 * @details При исключении останавливает context, дожидается всех потоков и повторно
 * выбрасывает первое исключение. Нулевое число потоков означает один поток.
 */
inline void RunWorkers(boost::asio::io_context& context, const unsigned count)
{
    std::exception_ptr failure;
    std::mutex mutex;
    auto run = [&]
    {
        try
        {
            context.run();
        }
        catch(...)
        {
            {
                std::lock_guard lock(mutex);
                if (!failure)
                {
                    failure = std::current_exception();
                }
            }
            context.stop();
        }
    };
    {
        std::vector<std::jthread> workers;
        const unsigned worker_count = std::max(1u, count);
        workers.reserve(worker_count - 1);
        try
        {
            for(unsigned i = 1; i < worker_count; ++i)
            {
                workers.emplace_back(run);
            }
        }
        catch(...)
        {
            context.stop();
            throw;
        }
        run();
    }
    if (failure)
    {
        std::rethrow_exception(failure);
    }
}
} // namespace http_server
