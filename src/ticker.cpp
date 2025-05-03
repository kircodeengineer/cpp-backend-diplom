#include "ticker.h"

namespace ticker {
	void Ticker::Start() {
		net::dispatch(strand_, [self = shared_from_this()]{
		  self->last_tick_ = Clock::now();
		  self->ScheduleTick();
			});
	}

	void Ticker::ScheduleTick() {
		if (!strand_.running_in_this_thread()) {
			throw std::runtime_error("Thread not running");
		}

		// Таймер сработает спустя заданный интервал относительно текущего момента
		timer_.expires_after(period_);
		// Таймер сработает, как только наступит заданный момент времени
		// timer_.steady_timer::expires_at.

		timer_.async_wait(
			[self = shared_from_this()](sys::error_code ec) { self->OnTick(ec); });
	}

	void Ticker::OnTick(sys::error_code ec) {
		using namespace std::chrono;
		if (!strand_.running_in_this_thread()) {
			throw std::runtime_error("Thread not running");
		}

		if (!ec) {
			auto this_tick = Clock::now();
			auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
			last_tick_ = this_tick;
			try {
				handler_(delta);
			}
			catch (...) {
			}
			ScheduleTick();
		}
	}
}  // namespace ticker
