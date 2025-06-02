#define BOOST_BEAST_USE_STD_STRING_VIEW
#include "json_loader.h"

#include <boost/json/src.hpp>
#include <fstream>
#include <iostream>

namespace json_loader {
	bool ReadFile(std::filesystem::path path, std::string& result) {
		// Open the stream to 'lock' the file.
		std::ifstream f(path, std::ios::in | std::ios::binary);

		if (!f.is_open()) {
			return false;
		}

		// Obtain the size of the file.
		const auto sz = std::filesystem::file_size(path);

		// Resize a buffer.
		result.resize(sz);

		// Read the whole file into the buffer.
		f.read(result.data(), sz);

		return true;
	}

	/// @brief добавление на карту дорог из json конфиг-файла
	/// @param map_json json конфиг-файл
	/// @param map карта
	void AddRoadsOnMap(const boost::json::value& map_json, model::Map& map) {
		if (map_json.as_object().if_contains(std::string(model::Literals::ROADS))) {
			for (std::size_t j = 0;
				j < map_json.at(std::string(model::Literals::ROADS)).as_array().size();
				++j) {
				auto road_json = map_json.at(std::string(model::Literals::ROADS)).at(j);
				// Горизонтальный отрезок дороги
				if (road_json.as_object().if_contains(std::string(model::Literals::X1))) {
					model::Point point{
						static_cast<int>(
							road_json.at(std::string(model::Literals::X0)).as_int64()),
						static_cast<int>(
							road_json.at(std::string(model::Literals::Y0)).as_int64()) };
					model::Coord end{ static_cast<int>(
						road_json.at(std::string(model::Literals::X1)).as_int64()) };
					model::Road road(model::Road::HORIZONTAL, point, end);
					map.AddRoad(road);
				} else {  // Вертикальный отрезок дороги
					model::Point point{
						static_cast<int>(
							road_json.at(std::string(model::Literals::X0)).as_int64()),
						static_cast<int>(
							road_json.at(std::string(model::Literals::Y0)).as_int64()) };
					model::Coord end{ static_cast<int>(
						road_json.at(std::string(model::Literals::Y1)).as_int64()) };
					model::Road road(model::Road::VERTICAL, point, end);
					map.AddRoad(road);
				}
			}
		}
	}

	/// @brief добавление на карту зданий из json конфиг-файла
	/// @param map_json json конфиг-файл
	/// @param map карта
	void AddBuildingsOnMap(const boost::json::value& map_json, model::Map& map) {
		if (map_json.as_object().if_contains(
			std::string(model::Literals::BUILDINGS))) {
			for (std::size_t j = 0; j < map_json.at(std::string(model::Literals::BUILDINGS)).as_array().size(); ++j) {
				auto building_json = map_json.at(std::string(model::Literals::BUILDINGS)).at(j);
				model::Point point{ static_cast<int>(building_json.at(std::string(model::Literals::X)).as_int64()),
					static_cast<int>(building_json.at(std::string(model::Literals::Y)).as_int64()) };
				model::Size size{ static_cast<int>(building_json.at(std::string(model::Literals::W)).as_int64()),
					static_cast<int>(building_json.at(std::string(model::Literals::H)).as_int64()) };
				model::Rectangle rectangle{ point, size };
				model::Building building{ rectangle };
				map.AddBuilding(building);
			}
		}
	}

	/// @brief добавление на карту офисов из json конфиг-файла
	/// @param map_json json конфиг-файл
	/// @param map карта
	void AddOfficesOnMap(const boost::json::value& map_json, model::Map& map) {
		if (map_json.as_object().if_contains(std::string(model::Literals::OFFICES))) {
			for (std::size_t j = 0; j < map_json.at(std::string(model::Literals::OFFICES)).as_array().size(); ++j) {
				auto office_jason = map_json.at(std::string(model::Literals::OFFICES)).at(j);
				model::Point point{
					static_cast<int>(office_jason.at(std::string(model::Literals::X)).as_int64()),
					static_cast<int>(office_jason.at(std::string(model::Literals::Y)).as_int64()) };
				model::Offset offset{
					static_cast<int>(office_jason.at(std::string(model::Literals::OFFSET_X)).as_int64()),
					static_cast<int>(office_jason.at(std::string(model::Literals::OFFSET_Y)).as_int64()) };
				std::string id_str(
					office_jason.at(std::string(model::Literals::ID)).as_string());
				model::Office::Id id{ id_str };
				model::Office office{ id, point, offset };
				map.AddOffice(office);
			}
		}
	}

	/// @brief добавление на карту массив объектов с произвольным содержимым из json
	/// конфиг-файла
	/// @param map_json json конфиг-файл
	/// @param map карта
	void AddLootTypesOnMap(const boost::json::value& map_json, model::Map& map) {
		if (map_json.as_object().if_contains(std::string(model::Literals::LOOT_TYPES))) {
			for (std::size_t j = 0; j < map_json.at(std::string(model::Literals::LOOT_TYPES)).as_array().size(); ++j) {
				model::LootType loot_type;
				auto loot_type_jason = map_json.at(std::string(model::Literals::LOOT_TYPES)).at(j);
				if (loot_type_jason.at(std::string(model::Literals::NAME)).as_string() == model::LootTypesNames::WALLET) {
					loot_type.name = model::LootTypesNames::WALLET;
					loot_type.file = model::LootTypesFiles::WALLET;
				}
				if (loot_type_jason.as_object().if_contains(std::string(model::Literals::ROTATION))) {
					loot_type.rotation = loot_type_jason.at(std::string(model::Literals::ROTATION)).as_int64();
				}

				if (loot_type_jason.as_object().if_contains(std::string(model::Literals::COLOR))) {
					loot_type.color = loot_type_jason.at(std::string(model::Literals::COLOR)).as_string();
				}
				loot_type.scale = loot_type_jason.at(std::string(model::Literals::SCALE)).as_double();
				loot_type.value = loot_type_jason.at(std::string(model::Literals::VALUE)).as_int64();
				map.AddLootType(loot_type);
			};

		}
	}

	void LoadGame(model::Game& game, const std::filesystem::path& json_path) {

		// Загрузить содержимое файла json_path, например, в виде строки
		std::string config_str;
		if (!ReadFile(json_path, config_str)) {
			return;
		}

		// Распарсить строку как JSON, используя boost::json::parse
		auto config_json = boost::json::parse(config_str);

		// Скорость игрока по-умолчанию
		if (config_json.as_object().if_contains("defaultDogSpeed")) {
			game.default_dog_speed = config_json.at("defaultDogSpeed").as_double();
		}

		// Настройки лут-генератора
		if (config_json.as_object().if_contains("lootGeneratorConfig")) {
			std::chrono::milliseconds period_ms{
				static_cast<uint64_t>(config_json.as_object().at("lootGeneratorConfig").as_object().at("period").as_double() * 1000.0) };
			loot_gen::LootGenerator loot_generator{ period_ms, config_json.as_object().at("lootGeneratorConfig").as_object().at("probability").as_double() };
			game.loot_generator_ = std::move(loot_generator);
		}

		// Вместимость рюкзаков по-умолчанию
		if (config_json.as_object().if_contains("defaultBagCapacity")) {
			game.default_bag_capacity_ = config_json.at("defaultBagCapacity").as_int64();
		}


		// Добавить карты в игру
		for (std::size_t i = 0; i < config_json.at(std::string(model::Literals::MAPS)).as_array().size(); ++i) {
			auto map_json = config_json.at(std::string(model::Literals::MAPS)).as_array().at(i);
			std::string id_str(map_json.at(std::string(model::Literals::ID)).as_string());
			model::Map::Id id_map(id_str);
			std::string name(map_json.at(std::string(model::Literals::NAME)).as_string());
			model::Map map(id_map, name);
			map.dog_speed = game.default_dog_speed;
			if (map_json.as_object().if_contains("dogSpeed")) {
				map.dog_speed = map_json.at("dogSpeed").as_double();
			}
			// Дороги
			AddRoadsOnMap(map_json, map);
			// Здания
			AddBuildingsOnMap(map_json, map);
			// Офисы
			AddOfficesOnMap(map_json, map);
			// Лут
			AddLootTypesOnMap(map_json, map);
			// Вместимость на карте
			if (map_json.as_object().if_contains("bagCapacity")) {
				map.bag_capacity_ = map_json.at("bagCapacity").as_int64();
			}
			game.AddMap(map);
		}
	}

}  // namespace json_loader
