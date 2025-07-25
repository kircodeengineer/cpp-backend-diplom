#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>

namespace ticker {
	namespace net = boost::asio;
	namespace sys = boost::system;

	// Класс периодически выполняет некоторую функцию внутри strand.
	// Ticker позволяет с заданной периодичностью обновлять игровое состояние, вызывая метод Tick.
	// Важно вызывать Tick в том же strand, в котором происходит обращение к вызовам API, чтобы не было состояния гонки
	class Ticker : public std::enable_shared_from_this<Ticker> {
	public:
		using Strand = net::strand<net::io_context::executor_type>;
		using Handler = std::function<void(std::chrono::milliseconds delta)>;

	private:
		using Clock = std::chrono::steady_clock;

		Strand strand_;
		std::chrono::milliseconds period_;
		net::steady_timer timer_{ strand_ };
		Handler handler_;
		std::chrono::steady_clock::time_point last_tick_;

	public:
		// Функция handler будет вызываться внутри strand с интервалом period
		Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
			: strand_{ strand }
			, period_{ period }
			, handler_{ std::move(handler) } {
		}

		/// @brief 
		void Start();

	private:
		/// @brief 
		void ScheduleTick();

		/// @brief 
		/// @param ec 
		void OnTick(sys::error_code ec);
	};
};
