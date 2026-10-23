#pragma once

#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

namespace ticker
{
namespace net = boost::asio;
namespace sys = boost::system;

/**
 * @brief Sequentially passes the actual elapsed time between ticks to the application.
 * @details The timer and handler share the application strand. Handler exceptions
 * propagate through io_context and stop the worker threads through RunWorkers.
 */
class Ticker : public std::enable_shared_from_this<Ticker>
{
public:
    /** @brief Sequential executor for the shared game operation queue. */
    using Strand = net::strand<net::io_context::executor_type>;
    /** @brief Receives the actual elapsed interval between ticks in milliseconds. */
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    /** @brief Accepts a positive interval and a game-time handler. */
    Ticker(const Strand strand, const std::chrono::milliseconds period, Handler handler):
            m_strand(strand),
            m_period(period),
            m_handler(std::move(handler))
    {
        if (period <= std::chrono::milliseconds::zero() || !m_handler)
        {
            throw std::invalid_argument("Ticker requires a positive period and a handler");
        }
    }

    /** @brief Disables copying the timer and its handlers. */
    Ticker(const Ticker&) = delete;
    /** @brief Disables moving the timer once handlers are bound. */
    Ticker(Ticker&&) = delete;
    /** @brief Disables copy assignment of an active timer. */
    Ticker& operator=(const Ticker&) = delete;
    /** @brief Disables moving an active timer. */
    Ticker& operator=(Ticker&&) = delete;

    /** @brief Starts the timer once after creation through shared_ptr. */
    void Start()
    {
        net::dispatch(m_strand,
                [self = shared_from_this()]
                {
                    if (self->m_started)
                    {
                        return;
                    }
                    /** @brief Prevents repeated Start calls from creating a second wait loop. */
                    self->m_started = true;
                    self->m_last_tick = Clock::now();
                    self->ScheduleTick();
                });
    }

private:
    /** @brief Monotonic clock unaffected by system time adjustments. */
    using Clock = std::chrono::steady_clock;
    /** @brief Sequential executor shared with the application. */
    Strand m_strand;
    /** @brief Positive wait interval before the next tick. */
    std::chrono::milliseconds m_period;
    /** @brief Owned timer invoking handlers on the game strand. */
    net::steady_timer m_timer{m_strand};
    /** @brief Operation advancing the application by the elapsed interval. */
    Handler m_handler;
    /** @brief Monotonic clock reading at the last delivered tick. */
    Clock::time_point m_last_tick{};
    /** @brief Prevents repeated Start calls from creating a second wait loop. */
    bool m_started = false;

    /** @brief Schedules the next tick wait on the game strand. */
    void ScheduleTick()
    {
        assert(m_strand.running_in_this_thread());
        m_timer.expires_after(m_period);
        m_timer.async_wait(
                [self = shared_from_this()](const sys::error_code error)
                {
                    self->OnTick(error);
                });
    }

    /** @brief Passes the monotonic interval to the application; does not reschedule a canceled wait. */
    void OnTick(const sys::error_code error)
    {
        assert(m_strand.running_in_this_thread());
        if (error)
        {
            return;
        }
        const auto now = Clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_tick);
        /** @brief Monotonic clock reading at the last delivered tick. */
        m_last_tick = now;
        m_handler(elapsed);
        ScheduleTick();
    }
};
} // namespace ticker
