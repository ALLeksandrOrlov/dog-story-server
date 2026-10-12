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
 * @brief Последовательно передаёт приложению реально прошедшее время между тиками.
 * @details Таймер и обработчик используют общий strand приложения. Исключение обработчика
 * передаётся io_context и завершает рабочие потоки через RunWorkers.
 */
class Ticker : public std::enable_shared_from_this<Ticker>
{
public:
    /** @brief Последовательный исполнитель общей очереди игровых операций. */
    using Strand = net::strand<net::io_context::executor_type>;
    /** @brief Получатель реально прошедшего между тиками интервала в миллисекундах. */
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    /** @brief Принимает положительный период и обработчик игрового времени. */
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

    /** @brief Таймер и его обработчики не копируются. */
    Ticker(const Ticker&) = delete;
    /** @brief Таймер не переносится после привязки обработчиков. */
    Ticker(Ticker&&) = delete;
    /** @brief Присваивание активного таймера запрещено. */
    Ticker& operator=(const Ticker&) = delete;
    /** @brief Перенос активного таймера запрещён. */
    Ticker& operator=(Ticker&&) = delete;

    /** @brief Запускает таймер один раз после создания через shared_ptr. */
    void Start()
    {
        net::dispatch(m_strand,
                [self = shared_from_this()]
                {
                    if (self->m_started)
                    {
                        return;
                    }
                    /** @brief Не позволяет повторному Start создать второй цикл ожидания. */
                    self->m_started = true;
                    self->m_last_tick = Clock::now();
                    self->ScheduleTick();
                });
    }

private:
    /** @brief Монотонные часы, не зависящие от перевода системного времени. */
    using Clock = std::chrono::steady_clock;
    /** @brief Общий с приложением последовательный исполнитель. */
    Strand m_strand;
    /** @brief Положительный интервал ожидания перед следующим тиком. */
    std::chrono::milliseconds m_period;
    /** @brief Владеющий таймер, вызывающий обработчики на игровом strand. */
    net::steady_timer m_timer{m_strand};
    /** @brief Операция продвижения приложения на прошедший интервал. */
    Handler m_handler;
    /** @brief Показание монотонных часов при последнем переданном тике. */
    Clock::time_point m_last_tick{};
    /** @brief Не позволяет повторному Start создать второй цикл ожидания. */
    bool m_started = false;

    /** @brief Планирует ожидание следующего тика на игровом strand. */
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

    /** @brief Передаёт монотонный интервал приложению; отменённое ожидание не повторяет. */
    void OnTick(const sys::error_code error)
    {
        assert(m_strand.running_in_this_thread());
        if (error)
        {
            return;
        }
        const auto now = Clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_tick);
        /** @brief Показание монотонных часов при последнем переданном тике. */
        m_last_tick = now;
        m_handler(elapsed);
        ScheduleTick();
    }
};
} // namespace ticker
