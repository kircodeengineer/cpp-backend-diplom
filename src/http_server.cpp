#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {
	using namespace std::literals;

	void ReportError(beast::error_code ec, std::string_view what) {
		// std::cerr << what << ": " << ec.message() << std::endl;
		using namespace boost::posix_time;
		auto report_time = to_iso_extended_string(microsec_clock::universal_time());
		using namespace boost::json;
		object obj;
		obj[std::string(logger::Literals::TIMESTAMP)] = report_time;
		obj[std::string(logger::Literals::DATA)] = {
			{std::string(logger::Literals::CODE), ec.value()},
			{std::string(logger::Literals::TEXT), ec.message()},
			{std::string(logger::Literals::WHERE), what} };
		obj[std::string(logger::Literals::MESSAGE)] = "error"s;
		LOG(serialize(obj));
	}

	SessionBase::SessionBase(tcp::socket&& socket) : stream_(std::move(socket)) {
		ip_ = stream_.socket().remote_endpoint().address().to_string();
	}

	void SessionBase::Run() {
		// Так как метод SessionBase::Run мы вызвали внутри strand, связанного с
		// акцептором, желательно как можно быстрее выйти из обработчика и начать
		// ожидать подключение следующего клиента. Поэтому дальнейшую работу с сокетом
		// будем проводить в его собственном strand. Воспользуемся функцией
		// boost::asio::dispatch, чтобы вызвать метод Session::Read внутри strand
		// сокета. Не забудем захватить std::shared_ptr на текущий объект.

		// Вызываем метод Read, используя executor объекта strean_
		// Таким образом вся работа со stream_ будет выполняться, используя его
		// executor
		net::dispatch(stream_.get_executor(),
			beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));

		// Среди перегрузок функции net::dispatch есть и та, которая принимает только
		// объект-обработчик. В этом случае функция выполнит вызов, используя
		// системный executor, который может выполнить вызов либо синхронно, либо
		// асинхронно. Параллельно с методом Read другие методы класса SessionBase не
		// вызываются. Поэтому вызов будет безопасным, хоть и произойдёт вне strand,
		// связанного с сокетом.
	}

	void SessionBase::Read() {
		// Очищаем запрос от прежнего значения
		request_ = {};

		stream_.expires_after(30s);

		// Считываем request_ из stream_, используя buffer_ для хранения считанных
		// данных
		http::async_read(
			stream_, buffer_, request_,
			// По окончании операции будет вызван метод OnRead
			beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
	}

	void SessionBase::OnRead(beast::error_code ec,
		[[maybe_unused]] std::size_t bytes_read) {
		// В OnRead в возможны три ситуации:
		// 1) Если клиент закрыл соединение, то сервер должен завершить сеанс.
		if (ec == http::error::end_of_stream) {
			return Close();
		}
		// 2) Если произошла ошибка чтения, выведите её в stdout.
		if (ec) {
			return ReportError(ec, "read"sv);
		}
		// 3) Если запрос прочитан без ошибок, делегируйте его обработку классу -
		// наследнику.
		HandleRequest(std::move(request_));
	}

	void SessionBase::OnWrite(bool close, beast::error_code ec,
		[[maybe_unused]] std::size_t bytes_written) {
		// Если при отправке ответа возникла ошибка, остаётся только завершить обмен
		// данными с клиентом.
		if (ec) {
			return ReportError(ec, "write"sv);
		}
		// В противном случае в зависимости от параметра close закройте соединение
		// либо продолжите обмен данными с клиентом.
		if (close) {
			return Close();
		}

		// считываем следующий запрос
		Read();
	}

	void SessionBase::Close() {
		beast::error_code ec;
		stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
		if (ec) {
			ReportError(ec, "shutdown socket"sv);
		}
	}
}  // namespace http_server
