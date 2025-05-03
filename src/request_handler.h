#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/io_context.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <iostream>
#include <variant>

#include "http_server.h"
#include "model.h"
#include "postgres.h"

namespace http_handler {
	using namespace boost::posix_time;
	using namespace boost::json;
	// Чтобы использовать литералы ""s и ""sv стандартной библиотеки, применим
	// std::literals.
	using namespace std::literals;
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace net = boost::asio;
	namespace fs = std::filesystem;
	using Strand = net::strand<net::io_context::executor_type>;

	// Ответ, тело которого представлено в виде строки
	using StringResponse = http::response<http::string_body>;
	// Ответ, тело которого представлено в виде файла
	using FileResponse = http::response<http::file_body>;
	// Ответ, тело которого отсутствует
	using EmptyResponse = http::response<http::empty_body>;
	// Запрос, тело которого представлено в виде строки
	using StringRequest = http::request<http::string_body>;

	struct Literals {
		Literals() = delete;
		constexpr static std::string_view API_MAPS = "/api/v1/maps"sv;
		constexpr static std::string_view API_MAP = "/api/v1/maps/"sv;
		constexpr static std::string_view API = "/api/"sv;
		constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
		constexpr static std::string_view API_JOIN = "/api/v1/game/join"sv;
		constexpr static std::string_view API_PLAYERS = "/api/v1/game/players"sv;
		constexpr static std::string_view API_STATE = "/api/v1/game/state"sv;
		constexpr static std::string_view API_ACTION = "/api/v1/game/player/action"sv;
		constexpr static std::string_view API_TICK = "/api/v1/game/tick"sv;
		constexpr static std::string_view API_RECORDS = "/api/v1/game/records"sv;
	};

	// Структура ContentType задаёт область видимости для констант,
	// задающий значения HTTP-заголовка Content-Type
	struct ContentType {
		ContentType() = delete;
		constexpr static std::string_view API_JSON = "application/json"sv;
		constexpr static std::string_view TEXT_HTML = "text/html"sv;
		constexpr static std::string_view TEXT = "text/plain"sv;
		// При необходимости внутрь ContentType можно добавить и другие типы контента
	};

	/// @brief Создаёт StringResponse с заданными параметрами
	/// @param status
	/// @param body
	/// @param http_version
	/// @param keep_alive
	/// @param method
	/// @param content_type
	/// @return
	StringResponse MakeStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	/// @brief
	/// @param status
	/// @param body
	/// @param http_version
	/// @param keep_alive
	/// @param method
	/// @param content_type
	/// @return
	StringResponse MakeJoinStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	/// @brief
	/// @param status
	/// @param body
	/// @param http_version
	/// @param keep_alive
	/// @param method
	/// @param content_type
	/// @return
	StringResponse MakePlayersStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	/// @brief
	/// @param status
	/// @param body
	/// @param http_version
	/// @param keep_alive
	/// @param method
	/// @param content_type
	/// @return
	StringResponse MakeStateStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	/// @brief
	/// @param status
	/// @param body
	/// @param http_version
	/// @param keep_alive
	/// @param method
	/// @param content_type
	/// @return
	StringResponse MakeActionStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	StringResponse MakeTickStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	StringResponse MakeRecordsStringResponse(
		http::status status, std::string_view body, unsigned http_version,
		bool keep_alive, http::verb method,
		std::string_view content_type = ContentType::API_JSON);

	// Создаёт FileResponse с заданными параметрами
	FileResponse MakeStaticFileResponse(http::status status,
		http::file_body::value_type&& file,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type);

	// полный ответ сервера
	struct StatusAndResponse {
		http::status http_status;
		std::string body;
	};

	// полный ответ с файлом от сервера
	struct StatusAndFileResponse {
		http::status http_status;
		http::file_body::value_type file;
		std::string content_type;
	};

	/// @brief Запрос содержит api?
	/// @param request_target
	/// @return
	bool IsApiRequest(std::string_view request_target);

	class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
	private:
		model::Game& game_;
		postgres::ConnectionPool& connection_pool_;
		fs::path root_;
		Strand api_strand_;
		std::string ip_{};

	public:
		explicit RequestHandler(model::Game& game, postgres::ConnectionPool& connection_pool, fs::path root, Strand api_strand)
			: game_{ game }, connection_pool_(connection_pool), root_{ std::move(root) }, api_strand_{ api_strand } {}

		RequestHandler(const RequestHandler&) = delete;
		RequestHandler& operator=(const RequestHandler&) = delete;

		template <typename Body, typename Allocator, typename Send>
		void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
			std::string ip, Send&& send) {
			auto version = req.version();
			auto keep_alive = req.keep_alive();
			ip_ = std::move(ip);
			LogRequest(req, ip_);

			try {
				if (IsApiRequest(req.target())) {
					auto handle = [self = shared_from_this(), send,
						req = std::forward<decltype(req)>(req), version,
						keep_alive]{
		   try {
						// Этот assert не выстрелит, так как лямбда-функция будет
						// выполняться внутри strand
						assert(self->api_strand_.running_in_this_thread());
						return send(self->HandleApiRequest(req));
					  }
			 catch (...) {
			send(self->ReportServerError(version, keep_alive));
		  }
					};
					return net::dispatch(api_strand_, handle);
				}
				// Возвращаем результат обработки запроса к файлу
				return std::visit(
					[&send](auto&& result) {
						send(std::forward<decltype(result)>(result));
					},
					HandleFileRequest(req));
			}
			catch (...) {
				send(ReportServerError(version, keep_alive));
			}
		}

	private:
		/// @brief Логгирование ответа сервера
		/// @param ip
		/// @param request_time
		/// @param code
		/// @param content_type
		void LogResponse(const std::string& ip,
			const std::chrono::system_clock::time_point& request_time,
			http::status code, std::string_view content_type);

		/// @brief Логгирование запроса
		/// @tparam Body
		/// @tparam Allocator
		/// @param request Запрос
		template <typename Body, typename Allocator>
		void LogRequest(
			const http::request<Body, http::basic_fields<Allocator>>& request,
			const std::string& ip) {
			auto request_time =
				to_iso_extended_string(microsec_clock::universal_time());
			object obj;
			obj[std::string(logger::Literals::TIMESTAMP)] = request_time;
			std::string method{ "NONE"s };
			if (request.method() == http::verb::get) {
				method = "GET"s;
			} else if (request.method() == http::verb::head) {
				method = "HEAD"s;
			} else if (request.method() == http::verb::post) {
				method = "POST"s;
			}
			obj[std::string(logger::Literals::DATA)] = {
				{std::string(logger::Literals::IP), ip},
				{std::string(logger::Literals::URI), request.target()},
				{std::string(logger::Literals::METHOD), method} };
			obj[std::string(logger::Literals::MESSAGE)] = "request received"s;
			LOG(serialize(obj));
		}

		/// @brief генерация ответа на запрос краткой онформации обо всех всех картах
		/// @param response ответ
		void GenerateMapsResponse(StatusAndResponse& response);

		/// @brief генерация ответа на запрос конкретной карты
		/// @param map_id_str id карты
		/// @param response ответ
		void GenerateMapResponse(const std::string& map_id_str,
			StatusAndResponse& response);

		/// @brief генерация ответа на плохой запрос
		/// @param response ответ на плохой запрос
		void GenerateBadRequestResponse(StatusAndResponse& response);

		/// @brief генерация ответа со статическим файлом
		/// @param response ответ
		void GenerateStaticFileResponse(std::string_view target,
			StatusAndFileResponse& response);

		/// @brief генерация ответа на запрос
		/// @param target запрос
		/// @param response ответ на запрос
		void GenerateResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief генерация ответ на вход в игру
		/// @param request
		/// @param response
		void GenerateJoinResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief генерация ответа на запрос игроков на карте
		/// @param request
		/// @return
		void GeneratePlayersResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief генерация ответа на запрос состояния игры на карте
		/// @param request
		/// @return
		void GenerateStateResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief генерация ответа на запрос пермещения игрока
		/// @param request
		/// @return
		void GenerateActionResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief генерация ответа на изменение времени
		/// @param request
		/// @return
		void GenerateTickResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief генерация ответа на запрос к БД
		/// @param request
		/// @return
		void GenerateRecordsResponse(const StringRequest& request,
			StatusAndResponse& response);

		/// @brief Запрос к БД
		/// @param start целое число, задающее номер начального элемента (0 — начальный элемент).
		/// @param max_items целое число, задающее максимальное количество элементов. Если maxItems превышает 100, должна вернуться ошибка с кодом 400 Bad Request.
		/// @return body от ответа на запрос
		std::string GetRecordsFromDB(int start, int max_items);

		using FileRequestResult =
			std::variant<EmptyResponse, StringResponse, FileResponse>;

		FileRequestResult HandleFileRequest(const StringRequest& request) {
			auto request_time = std::chrono::system_clock::now();
			StatusAndFileResponse response;
			GenerateStaticFileResponse(request.target(), response);
			if (response.http_status == http::status::ok) {
				LogResponse(ip_, request_time, response.http_status,
					response.content_type);
				return MakeStaticFileResponse(
					response.http_status, std::move(response.file), request.version(),
					request.keep_alive(), request.method(), response.content_type);
			}
			LogResponse(ip_, request_time, response.http_status, ContentType::TEXT);
			return MakeStringResponse(response.http_status, "none", request.version(),
				request.keep_alive(), request.method(),
				ContentType::TEXT);

		}

		StringResponse HandleApiRequest(const StringRequest& request) {
			// if (request.method() != http::verb::get && request.method() !=
			// http::verb::head) {
			//}
			auto request_time = std::chrono::system_clock::now();
			// Обработать запрос request и отправить ответ, используя send
			StatusAndResponse response;
			if (request.target().find(Literals::API_JOIN) != std::string::npos) {
				GenerateJoinResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakeJoinStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			} else if (request.target().find(Literals::API_PLAYERS) !=
				std::string::npos) {
				GeneratePlayersResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakePlayersStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			} else if (request.target().find(Literals::API_STATE) !=
				std::string::npos) {
				GenerateStateResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakeStateStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			} else if (request.target().find(Literals::API_ACTION) !=
				std::string::npos) {
				GenerateActionResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakeActionStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			} else if (request.target().find(Literals::API_TICK) != std::string::npos) {
				GenerateTickResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakeTickStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			} else if (request.target().find(Literals::API_RECORDS) != std::string::npos) {
				GenerateRecordsResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakeStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			} else {
				GenerateResponse(request, response);
				LogResponse(ip_, request_time, response.http_status,
					ContentType::API_JSON);
				return MakeStringResponse(response.http_status, response.body,
					request.version(), request.keep_alive(),
					request.method(), ContentType::API_JSON);
			}
		}

		StringResponse ReportServerError(unsigned version, bool keep_alive) {
			StringResponse response(http::status::internal_server_error, version);
			response.set(http::field::content_type, ContentType::TEXT);
			response.keep_alive(keep_alive);
			auto request_time = std::chrono::system_clock::now();
			LogResponse(ip_, request_time, http::status::internal_server_error,
				ContentType::TEXT);
			return response;
		}

		/// @brief проверка наличия токена в БД
		/// @param token
		/// @param response
		/// @param map_name_str
		/// @return
		bool IsTokenUnknown(const std::string& token, StatusAndResponse& response,
			std::string& map_name_str);
	};

}  // namespace http_handler
