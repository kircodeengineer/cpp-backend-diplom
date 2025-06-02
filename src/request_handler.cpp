#define BOOST_BEAST_USE_STD_STRING_VIEW

#include "request_handler.h"

#include <boost/json.hpp>
#include <filesystem>
#include <random>

#include "random_functions.h"

namespace http_handler {
	using namespace boost::json;

	/// @brief декодировщик URI строки
	/// @param s
	/// @return
	std::string decode_url(const std::string& s) {
		std::string result;
		for (std::size_t i = 0; i < s.size(); i++) {
			if (s[i] == '%') {
				try {
					auto v = std::stoi(s.substr(i + 1, 2), nullptr, 16);
					result.push_back(0xff & v);
				}
				catch (...) {
				}  // handle error
				i += 2;
			} else {
				result.push_back(s[i]);
			}
		}
		return result;
	}

	StringResponse MakeStringResponse(http::status status, std::string_view body,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		StringResponse response(status, http_version);
		response.set(http::field::content_type, content_type);
		if (method != http::verb::head) {
			response.body() = body;
		}
		if (status == http::status::method_not_allowed) {
			response.set(http::field::allow, "GET, HEAD"sv);
		}
		response.set(http::field::cache_control, "no-cache"sv);  // test
		response.content_length(body.size());
		response.keep_alive(keep_alive);
		return response;
	}

	StringResponse MakeJoinStringResponse(http::status status,
		std::string_view body,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		StringResponse response(status, http_version);
		response.set(http::field::content_type, content_type);
		if (method != http::verb::head) {
			response.body() = body;
		}
		if (status == http::status::method_not_allowed) {
			response.set(http::field::allow, "POST"sv);
		}
		response.set(http::field::cache_control, "no-cache"sv);

		response.content_length(body.size());
		response.keep_alive(keep_alive);
		return response;
	}

	StringResponse MakePlayersStringResponse(http::status status,
		std::string_view body,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		StringResponse response(status, http_version);
		response.set(http::field::content_type, content_type);
		if (method != http::verb::head) {
			response.body() = body;
		}
		if (status == http::status::method_not_allowed) {
			response.set(http::field::allow, "GET, HEAD"sv);
		}
		response.set(http::field::cache_control, "no-cache"sv);

		response.content_length(body.size());
		response.keep_alive(keep_alive);
		return response;
	}

	StringResponse MakeStateStringResponse(http::status status,
		std::string_view body,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		return MakePlayersStringResponse(status, body, http_version, keep_alive,
			method, content_type);
	}

	StringResponse MakeActionStringResponse(http::status status,
		std::string_view body,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		return MakeJoinStringResponse(status, body, http_version, keep_alive, method,
			content_type);
	}

	StringResponse MakeTickStringResponse(http::status status,
		std::string_view body,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		StringResponse response(status, http_version);
		response.set(http::field::content_type, content_type);
		if (method != http::verb::head) {
			response.body() = body;
		}

		response.set(http::field::cache_control, "no-cache"sv);

		response.content_length(body.size());
		response.keep_alive(keep_alive);
		return response;
	}

	bool IsApiRequest(std::string_view request_target) {
		const std::string api_request = "/api/"s;
		if (request_target.size() >= api_request.size() &&
			request_target.substr(0, api_request.size()) == api_request) {
			return true;
		}
		return false;
	}

	void RequestHandler::GenerateMapsResponse(StatusAndResponse& response) {
		// запрос возвращает в теле ответа краткую информацию обо всех картах в виде
		// JSON-массива объектов с полями id и name
		array maps_arr;
		auto maps{ game_.GetMaps() };
		for (const auto& map : maps) {
			object obj;
			obj[std::string(model::Literals::ID)] = *map.GetId();
			obj[std::string(model::Literals::NAME)] = map.GetName();
			maps_arr.push_back(obj);
		}
		response.http_status = http::status::ok;
		response.body = serialize(maps_arr);
	}

	/// @brief добавление дорог в ответ на щапрос
	/// @param map_ptr карта
	/// @param obj ответ
	void AddRoadsToResponse(const model::Map* map_ptr, object& obj) {
		array roads_arr;
		auto roads{ map_ptr->GetRoads() };
		for (const auto& road : roads) {
			object road_obj;
			road_obj[std::string(model::Literals::X0)] = road.GetStart().x;
			road_obj[std::string(model::Literals::Y0)] = road.GetStart().y;
			if (road.IsHorizontal()) {
				road_obj[std::string(model::Literals::X1)] = road.GetEnd().x;
			} else {
				road_obj[std::string(model::Literals::Y1)] = road.GetEnd().y;
			}
			roads_arr.push_back(road_obj);
		}
		obj[std::string(model::Literals::ROADS)] = roads_arr;
	}

	/// @brief добавление зданий в ответ на запрос
	/// @param map_ptr карта
	/// @param obj ответ
	void AddBuildingsToResponse(const model::Map* map_ptr, object& obj) {
		array buildings_arr;
		auto buildings{ map_ptr->GetBuildings() };
		for (const auto& building : buildings) {
			object builing_obj;
			builing_obj[std::string(model::Literals::X)] = building.GetBounds().position.x;
			builing_obj[std::string(model::Literals::Y)] = building.GetBounds().position.y;
			builing_obj[std::string(model::Literals::W)] = building.GetBounds().size.width;
			builing_obj[std::string(model::Literals::H)] = building.GetBounds().size.height;
			buildings_arr.push_back(builing_obj);
		}
		obj[std::string(model::Literals::BUILDINGS)] = buildings_arr;
	}

	/// @brief добавление офисов в ответ на запрос
	/// @param map_ptr карта
	/// @param obj ответ
	void AddOfficesToResponse(const model::Map* map_ptr, object& obj) {
		array offices_arr;
		auto offices{ map_ptr->GetOffices() };
		for (const auto& office : offices) {
			object office_obj;
			office_obj[std::string(model::Literals::ID)] = *office.GetId();
			office_obj[std::string(model::Literals::X)] = office.GetPosition().x;
			office_obj[std::string(model::Literals::Y)] = office.GetPosition().y;
			office_obj[std::string(model::Literals::OFFSET_X)] = office.GetOffset().dx;
			office_obj[std::string(model::Literals::OFFSET_Y)] = office.GetOffset().dy;
			offices_arr.push_back(office_obj);
		}
		obj[std::string(model::Literals::OFFICES)] = offices_arr;
	}

	/// @brief Добавить типы лута в ответ на запрос
	/// @param map_ptr карта
	/// @param obj ответ
	void AddLootTypesToResponse(const model::Map* map_ptr, object& obj) {
		array loot_types_arr;
		auto loot_types{ map_ptr->GetLootTypes() };
		for (const auto& loot_type : loot_types) {
			object loot_type_obj;
			loot_type_obj[std::string(model::Literals::NAME)] = std::string(loot_type.name);
			loot_type_obj[std::string(model::Literals::FILE)] = std::string(loot_type.file);
			loot_type_obj[std::string(model::Literals::TYPE)] = std::string(loot_type.type);
			if (loot_type.rotation.has_value()) {
				loot_type_obj[std::string(model::Literals::ROTATION)] = loot_type.rotation.value();
			}
			if (loot_type.color.has_value()) {
				loot_type_obj[std::string(model::Literals::COLOR)] = std::string(loot_type.color.value());
			}
			loot_type_obj[std::string(model::Literals::SCALE)] = loot_type.scale;
			loot_type_obj[std::string(model::Literals::VALUE)] = loot_type.value;
			loot_types_arr.push_back(loot_type_obj);
		}
		obj[std::string(model::Literals::LOOT_TYPES)] = loot_types_arr;
	}

	void RequestHandler::GenerateMapResponse(const std::string& map_id_str, StatusAndResponse& response) {
		model::Map::Id map_id{ map_id_str };
		// запрос возвращает в теле ответа JSON-описание карты с указанным id,
		// семантически эквивалентное представлению карты из конфигурационного файла
		auto map_ptr = game_.FindMap(map_id);
		if (map_ptr != nullptr) {
			object obj;
			obj[std::string(model::Literals::ID)] = *map_ptr->GetId();
			obj[std::string(model::Literals::NAME)] = map_ptr->GetName();
			// Дороги
			AddRoadsToResponse(map_ptr, obj);
			// Здания
			AddBuildingsToResponse(map_ptr, obj);
			// Офисы
			AddOfficesToResponse(map_ptr, obj);
			// Типы лута
			AddLootTypesToResponse(map_ptr, obj);
			response.http_status = http::status::ok;
			response.body = serialize(obj);
		} else {
			object obj;
			obj[std::string(model::Literals::CODE)] = "mapNotFound";
			obj[std::string(model::Literals::MESSAGE)] = "Map not found";
			response.http_status = http::status::not_found;
			response.body = serialize(obj);
		}
	}

	void RequestHandler::GenerateBadRequestResponse(StatusAndResponse& response) {
		object obj;
		obj[std::string(model::Literals::CODE)] = "badRequest";
		obj[std::string(model::Literals::MESSAGE)] = "Bad request";
		response.http_status = http::status::bad_request;
		response.body = serialize(obj);
	}

	namespace fs = std::filesystem;
	/// @brief Возвращает true, если каталог p содержит внутри base_path
	/// @param path
	/// @param base
	/// @return
	bool IsSubPath(fs::path path, fs::path base) {
		// Приводим оба пути к каноничному виду (без . и ..)
		path = fs::weakly_canonical(path);
		base = fs::weakly_canonical(base);

		// Проверяем, что все компоненты base содержатся внутри path (папка за папкой)
		// *p Имя папки
		for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
			if (p == path.end() || *p != *b) {
				return false;
			}
		}
		return true;
	}

	FileResponse MakeStaticFileResponse(http::status status,
		http::file_body::value_type&& file,
		unsigned http_version, bool keep_alive,
		http::verb method,
		std::string_view content_type) {
		FileResponse response(status, http_version);
		response.set(http::field::content_type, content_type);
		response.body() = std::move(file);
		response.prepare_payload();
		return response;
	}

	/// @brief формировщик content-type по формату запрашиваемого файла
	/// @param file_format_sv
	/// @return
	std::string GetFileContentType(std::string_view file_format_sv) {
		std::string file_format{ file_format_sv };
		std::transform(file_format.begin(), file_format.end(), file_format.begin(),
			[](unsigned char c) { return std::tolower(c); });
		if (file_format.find(".htm"s) != std::string::npos ||
			file_format.find(".html"s) != std::string::npos) {
			return "text/html"s;
		} else if (file_format.find(".css"s) != std::string::npos) {
			return "text/css"s;
		} else if (file_format.find(".txt"s) != std::string::npos) {
			return "text/txt"s;
		} else if (file_format.find(".js"s) != std::string::npos) {
			return "text/javascript"s;
		} else if (file_format.find(".json"s) != std::string::npos) {
			return "application/json"s;
		} else if (file_format.find(".xml"s) != std::string::npos) {
			return "application/xml"s;
		} else if (file_format.find(".png"s) != std::string::npos) {
			return "image/png"s;
		} else if (file_format.find(".jpg"s) != std::string::npos ||
			file_format.find(".jpe"s) != std::string::npos ||
			file_format.find(".jpeg"s) != std::string::npos) {
			return "image/jpeg"s;
		} else if (file_format.find(".gif"s) != std::string::npos) {
			return "image/gif"s;
		} else if (file_format.find(".bmp"s) != std::string::npos) {
			return "image/bmp"s;
		} else if (file_format.find(".ico"s) != std::string::npos) {
			return "image/vnd.microsoft.icon"s;
		} else if (file_format.find(".tiff"s) != std::string::npos ||
			file_format.find(".tif"s) != std::string::npos) {
			return "image/tiff"s;
		} else if (file_format.find(".svg"s) != std::string::npos ||
			file_format.find(".svgz"s) != std::string::npos) {
			return "image/svg+xml"s;
		}
		// Для файлов с пустым или неизвестным расширением заголовок Content-Type
		// должен иметь значение application/octet-stream.
		return "application/octet-stream"s;
	}

	void RequestHandler::GenerateStaticFileResponse(
		std::string_view target, StatusAndFileResponse& response) {
		std::string request_path_str{ target };
		request_path_str = decode_url(request_path_str);
#ifdef WIN32
		std::replace(request_path_str.begin(), request_path_str.end(), '/', '\\');
#endif
		if (request_path_str == "/"s) {
			request_path_str += "index.html"s;
		}
#ifdef WIN32
		if (request_path_str == "\\"s) {
			request_path_str += "index.html"s;
		}
#endif
		fs::path request_path{ game_.GetStaticPath() + request_path_str };
		fs::path static_path{ game_.GetStaticPath() };
		if (IsSubPath(request_path, static_path)) {
			namespace sys = boost::system;
			using namespace http;
			file_body::value_type file;
			if (sys::error_code ec; file.open(request_path.generic_string().data(),
				beast::file_mode::read, ec),
				ec) {
				// Если URI-строка ссылается на несуществующий файл,
				// сервер должен вернуть ответ со статус-кодом 404 Not Found и
				// Content-Type: text/plain. Текст в ответе остаётся на ваше усмотрение
				response.http_status = http::status::not_found;
				response.content_type = Literals::TEXT_PLAIN;
			} else {
				response.http_status = http::status::ok;
				response.file = std::move(file);
				response.content_type = GetFileContentType(request_path_str);
			}
		} else {
			// Если результирующий путь к файлу оказался вне корневого каталога со
			// статическими файлами, сервер должен вернуть ответ со статус-кодом 400 Bad
			// Request и Content-Type: text/plain. Текст ошибки в ответе остаётся на
			// ваше усмотрение.
			response.http_status = http::status::bad_request;
			response.content_type = Literals::TEXT_PLAIN;
		}
	}

	/// @brief генерация ответа что-то пошло в не так внутри логики работы сервера
	/// @param response
	void GenerateSomethingWrongResponse(StatusAndResponse& response) {
		object obj;
		response.http_status = http::status::internal_server_error;
		obj[std::string(model::Literals::CODE)] = "internalError";
		obj[std::string(model::Literals::MESSAGE)] =
			"Something Wrong In Server Logic";
		response.body = serialize(obj);
	}

	void RequestHandler::GenerateJoinResponse(const StringRequest& request, StatusAndResponse& response) {
		object obj;
		if (request.method() != http::verb::post) {
			response.http_status = http::status::method_not_allowed;
			obj[std::string(model::Literals::CODE)] = "invalidMethod";
			obj[std::string(model::Literals::MESSAGE)] = "Only POST method is expected";
			response.body = serialize(obj);
			return;
		}

		try {
			auto config_json = boost::json::parse(request.body());

			if (config_json.at("userName"s).as_string().empty()) {
				response.http_status = http::status::bad_request;
				obj[std::string(model::Literals::CODE)] = "invalidArgument";
				obj[std::string(model::Literals::MESSAGE)] = "Invalid name";
				response.body = serialize(obj);
				return;
			}

			std::string map_name{ config_json.at("mapId"s).as_string().c_str() };
			model::Map::Id map_id{ map_name };
			auto map_ptr = game_.FindMap(map_id);
			if (map_ptr == nullptr) {
				response.http_status = http::status::not_found;
				obj[std::string(model::Literals::CODE)] = "mapNotFound";
				obj[std::string(model::Literals::MESSAGE)] = "Map not found";
				response.body = serialize(obj);
				return;
			}
			std::string hash{ random_functions::random_hex_string(32) };
			obj["authToken"] = hash;
			obj["playerId"] = game_.AddPlayerOnMap(map_ptr, map_name, hash, config_json.at("userName"s).as_string().c_str());

			response.http_status = http::status::ok;
			response.body = serialize(obj);
		}
		catch (...) {
			response.http_status = http::status::bad_request;
			obj[std::string(model::Literals::CODE)] = "invalidArgument";
			obj[std::string(model::Literals::MESSAGE)] =
				"Join game request parse error";
			response.body = serialize(obj);
		}
	}

	/// @brief генерация ответа на неверный метод
	/// @param response
	void GenerateInvalidMethodResponse(StatusAndResponse& response) {
		object obj;
		response.http_status = http::status::method_not_allowed;
		obj[std::string(model::Literals::CODE)] = "invalidMethod";
		obj[std::string(model::Literals::MESSAGE)] = "Invalid method";
		response.body = serialize(obj);
	}



	/// @brief проверка соответствия токена требованиям
	/// @param request
	/// @param response
	/// @param last_word последнее слово возвращаемого сообщения
	/// @param token
	/// @return
	bool IsTokenInvalid(const StringRequest& request, StatusAndResponse& response,
		const std::string& last_word, std::string& token) {
		object obj;
		std::string bearer{ "Bearer "s };
		bool is_authorization_exist = false;
		for (auto& h : request.base()) {
			std::string token_str{ h.name_string() };
			std::transform(token_str.begin(), token_str.end(), token_str.begin(),
				[](unsigned char c) { return std::tolower(c); });

			if (token_str == "authorization"s) {
				is_authorization_exist = true;
				token = h.value().substr(bearer.size());
				break;
			}
		}

		if (!is_authorization_exist) {
			response.http_status = http::status::unauthorized;
			obj[std::string(model::Literals::CODE)] = "invalidToken"s;
			obj[std::string(model::Literals::MESSAGE)] =
				"Authorization header is "s + last_word;
			response.body = serialize(obj);
			return true;
		}

		return false;
	}

	/// @brief проверка наличия токена в БД
	/// @param token
	/// @param response
	/// @param map_name_str
	/// @return
	bool RequestHandler::IsTokenUnknown(const std::string& token,
		StatusAndResponse& response,
		std::string& map_name_str) {
		object obj;
		auto map_name = game_.GetMapNameByHash(token);
		if (map_name == nullptr) {
			response.http_status = http::status::unauthorized;
			obj[std::string(model::Literals::CODE)] = "unknownToken";
			obj[std::string(model::Literals::MESSAGE)] =
				"Player token has not been found";
			response.body = serialize(obj);
			return true;
		}
		map_name_str = *map_name;
		return false;
	}

	object GetPlayersData(const std::string& map_name, model::Game& game) {
		std::deque<model::Player> players_on_map;
		game.GetPlayersOnMap(players_on_map, map_name);
		object players;
		for (const auto& player : players_on_map) {
			array pos;
			pos.push_back(player.pos_.x);
			pos.push_back(player.pos_.y);
			array speed;
			speed.push_back(player.speed_.x);
			speed.push_back(player.speed_.y);
			array bag;
			object loot;
			for (const auto loot_with_id : player.bag_) {
				loot["id"] = loot_with_id.id;
				loot["type"] = loot_with_id.type;
				bag.push_back(loot);
			}
			players[std::to_string(player.id_)] = {
			{"pos", pos},
			{"speed", speed},
			{"dir", model::DirectionToString(player.direction_)} ,
			{"bag" , bag },
			{"score" , player.score } };
		}
		return players;
	}

	object GetLootData(const std::string& map_name, model::Game& game) {
		std::deque<model::Loot> loot_on_map;
		game.GetLootOnMap(loot_on_map, map_name);
		object all_loot;
		unsigned int id{ 0 };
		for (const auto& loot : loot_on_map) {
			array pos;
			pos.push_back(loot.coord.x);
			pos.push_back(loot.coord.y);
			all_loot[std::to_string(id)] = {
				{"type", loot.type},
				{"pos", pos} };
			++id;
		}
		return all_loot;
	}

	void RequestHandler::GenerateStateResponse(const StringRequest& request,
		StatusAndResponse& response) {
		if (request.method() != http::verb::head &&
			request.method() != http::verb::get) {
			return GenerateInvalidMethodResponse(response);
		}

		std::string token{};
		if (IsTokenInvalid(request, response, "required", token)) {
			return;
		}

		object obj;
		std::string map_name;
		if (!IsTokenUnknown(token, response, map_name)) {
			obj["players"] = GetPlayersData(map_name, game_);
			obj["lostObjects"] = GetLootData(map_name, game_);
			response.http_status = http::status::ok;
			response.body = serialize(obj);
		}
	}

	void RequestHandler::GeneratePlayersResponse(const StringRequest& request,
		StatusAndResponse& response) {
		if (request.method() != http::verb::head &&
			request.method() != http::verb::get) {
			return GenerateInvalidMethodResponse(response);
			;
		}

		std::string token{};
		if (IsTokenInvalid(request, response, "missing", token)) {
			return;
		}

		object obj;
		std::string map_name;
		if (!IsTokenUnknown(token, response, map_name)) {
			std::deque<model::Player> players_on_map;
			game_.GetPlayersOnMap(players_on_map, map_name);
			for (const auto& player : players_on_map) {
				obj[std::to_string(player.id_)] = {
					"name", player.name_ };  // player.GetPlayerId(), player.GetName()
			}
			response.http_status = http::status::ok;
			response.body = serialize(obj);
		}
	}

	void RequestHandler::GenerateTickResponse(const StringRequest& request,
		StatusAndResponse& response) {
		if (request.method() != http::verb::post) {
			return GenerateInvalidMethodResponse(response);
		}

		object obj;
		if (game_.world_auto_update) {  // game_.IsWorldAutoUpdate()
			obj[std::string(model::Literals::CODE)] = "badRequest";
			obj[std::string(model::Literals::MESSAGE)] = "Invalid endpoint";
			response.http_status = http::status::bad_request;
			response.body = serialize(obj);
			return;
		}

		obj[std::string(model::Literals::CODE)] = "invalidArgument";
		obj[std::string(model::Literals::MESSAGE)] = "Failed to parse tick request JSON";
		response.http_status = http::status::bad_request;
		response.body = serialize(obj);

		if (request.body().empty()) {
			return;
		}

		auto time_tick_json = boost::json::parse(request.body());
		if (!time_tick_json.as_object().if_contains("timeDelta"s) ||
			!time_tick_json.at("timeDelta"s).is_int64() ||
			time_tick_json.at("timeDelta"s).as_int64() <= 0) {
			return;
		}

		std::chrono::milliseconds period_ms{ time_tick_json.at("timeDelta"s).as_int64() };
		game_.SpendTime(period_ms);

		obj.clear();
		response.http_status = http::status::ok;
		response.body = serialize(obj);
	}

	bool IsContentTypeOk(const StringRequest& request, StatusAndResponse& response,
		std::string content_type_for_check) {
		object obj;

		bool content_type_ok{ false };
		for (auto& h : request.base()) {
			std::string conytent_str{ h.name_string() };
			std::transform(conytent_str.begin(), conytent_str.end(),
				conytent_str.begin(),
				[](unsigned char c) { return std::tolower(c); });
			if (conytent_str == "content-type"s &&
				h.value() == content_type_for_check) {
				content_type_ok = true;
				break;
			}
		}

		if (!content_type_ok) {
			obj[std::string(model::Literals::CODE)] = "invalidArgument";
			obj[std::string(model::Literals::MESSAGE)] = "Invalid content type";
			response.http_status = http::status::bad_request;
			response.body = serialize(obj);
			return false;
		}

		return true;
	}

	void RequestHandler::GenerateActionResponse(const StringRequest& request,
		StatusAndResponse& response) {
		if (request.method() != http::verb::post) {
			return GenerateInvalidMethodResponse(response);
		}

		std::string token{};
		if (IsTokenInvalid(request, response, "required", token)) {
			return;
		}

		std::string map_name;
		if (IsTokenUnknown(token, response, map_name)) {
			return;
		}

		if (!IsContentTypeOk(request, response, "application/json"s)) {
			return;
		}

		object obj;
		auto action_json = boost::json::parse(request.body());
		game_.MovePlayer(action_json.at("move").as_string().c_str(), token, map_name);

		response.http_status = http::status::ok;
		response.body = serialize(obj);
	}

	void RequestHandler::GenerateResponse(const StringRequest& request,
		StatusAndResponse& response) {
		if (request.target() == Literals::API_MAPS) {
			return GenerateMapsResponse(response);
		} else if (request.target().find(Literals::API_MAP) != std::string::npos) {
			// TODO Убрать в метод
			object obj;
			if (request.method() != http::verb::head && request.method() != http::verb::get) {
				response.http_status = http::status::method_not_allowed;
				obj[std::string(model::Literals::CODE)] = "invalidMethod";
				obj[std::string(model::Literals::MESSAGE)] = "Only GET or HEAD methods are expected";
				response.body = serialize(obj);
				return;
			}
			return GenerateMapResponse(std::string(request.target().substr(13)), response);
		} else {
			if (request.target().find(Literals::API) != std::string::npos) {
				GenerateBadRequestResponse(response);
			}
		}
		response.http_status = http::status::bad_request;
	}

	void RequestHandler::LogResponse(
		const std::string& ip,
		const std::chrono::system_clock::time_point& request_time,
		http::status code, std::string_view content_type) {
		auto response_time = to_iso_extended_string(microsec_clock::universal_time());
		object obj;
		obj[std::string(logger::Literals::TIMESTAMP)] =
			to_iso_extended_string(microsec_clock::universal_time());
		auto time_res = std::chrono::system_clock::now();
		auto delta = time_res - request_time;
		auto delta_ms =
			std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
		obj[std::string(logger::Literals::DATA)] = {
			{std::string(logger::Literals::IP), ip},
			{std::string(logger::Literals::RESPONSE_TIME), delta_ms},
			{std::string(logger::Literals::CODE), static_cast<int>(code)},
			{std::string(logger::Literals::CONTENT_TYPE), content_type} };
		obj[std::string(logger::Literals::MESSAGE)] = "response sent"s;
		LOG(serialize(obj));
	}

}  // namespace http_handler
