#pragma once
#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "loot_generator.h"
#include "tagged.h"
#include "collision_detector.h"

namespace model {
	class Provider : public collision_detector::ItemGathererProvider {
	public:
		using Items = std::vector<collision_detector::Item>;
		using Gatherers = std::vector <collision_detector::Gatherer>;

		Provider() {};

		Provider(Items items, Gatherers gatherers)
			: items_(items)
			, gatherers_(gatherers) {}

		size_t ItemsCount() const {
			return items_.size();
		}

		collision_detector::Item GetItem(size_t idx) const {
			return items_.at(idx);
		}

		size_t GatherersCount() const {
			return gatherers_.size();
		}

		collision_detector::Gatherer GetGatherer(size_t idx) const {
			return gatherers_.at(idx);
		}

		void AddGatherer(collision_detector::Gatherer& new_gath) {
			gatherers_.emplace_back(new_gath);
		}

		void AddItem(collision_detector::Item& new_item) {
			items_.emplace_back(new_item);
		}

		const Gatherers& GetGatherers() {
			return gatherers_;
		}

		void ClearGatherers() {
			gatherers_.clear();
		}

		void ClearItems() {
			items_.clear();
		}

	private:
		Items items_;
		Gatherers gatherers_;
	};

	using namespace std::literals;

	struct Literals {
		Literals() = delete;
		constexpr static std::string_view BUILDINGS = "buildings"sv;
		constexpr static std::string_view OFFICES = "offices"sv;
		constexpr static std::string_view ROADS = "roads"sv;
		constexpr static std::string_view X0 = "x0"sv;
		constexpr static std::string_view X1 = "x1"sv;
		constexpr static std::string_view Y0 = "y0"sv;
		constexpr static std::string_view Y1 = "y1"sv;
		constexpr static std::string_view X = "x"sv;
		constexpr static std::string_view Y = "y"sv;
		constexpr static std::string_view W = "w"sv;
		constexpr static std::string_view H = "h"sv;
		constexpr static std::string_view OFFSET_X = "offsetX"sv;
		constexpr static std::string_view OFFSET_Y = "offsetY"sv;
		constexpr static std::string_view ID = "id"sv;
		constexpr static std::string_view MAPS = "maps"sv;
		constexpr static std::string_view NAME = "name"sv;
		constexpr static std::string_view CODE = "code"sv;
		constexpr static std::string_view MESSAGE = "message"sv;
		constexpr static std::string_view LOOT_TYPES = "lootTypes"sv;

		constexpr static std::string_view FILE = "file"sv;
		constexpr static std::string_view TYPE = "type"sv;
		constexpr static std::string_view ROTATION = "rotation"sv;
		constexpr static std::string_view COLOR = "color"sv;
		constexpr static std::string_view SCALE = "scale"sv;
		constexpr static std::string_view VALUE = "value"sv;
	};

	// имена типов лута
	struct LootTypesNames {
		LootTypesNames() = delete;
		constexpr static std::string_view KEY = "key"sv;
		constexpr static std::string_view WALLET = "wallet"sv;
	};

	// файлы типов лута
	struct LootTypesFiles {
		LootTypesFiles() = delete;
		constexpr static std::string_view KEY = "assets/key.obj"sv;
		constexpr static std::string_view WALLET = "assets/wallet.obj"sv;
	};

	struct MapObjTypes {
		MapObjTypes() = delete;
		constexpr static std::string_view OBJ = "obj"sv;
	};

	using Dimension = int;
	using Coord = Dimension;

	struct Point {
		Coord x, y;
	};

	struct Size {
		Dimension width, height;
	};

	struct Rectangle {
		Point position;
		Size size;
	};

	struct Offset {
		Dimension dx, dy;
	};

	struct RoadBorders {
		double left_border{ 0.0 };
		double right_border{ 0.0 };
		double up_border{ 0.0 };
		double down_border{ 0.0 };
	};

	struct FloatCoord {
		double x{ 0.0 };
		double y{ 0.0 };
	};



	// описание типа лута по config.json
	struct LootType {
		std::string_view name{ LootTypesNames::KEY };
		std::string_view file{ LootTypesFiles::KEY };
		std::string_view type{ MapObjTypes::OBJ };
		std::optional<uint64_t> rotation;
		std::optional<std::string> color;
		double scale{ 0.0 };
		long long value{ 0 };
	};

	// лут на карте
	struct Loot {
		// целое число, задающее тип объекта в диапазоне [0, N-1], где N — количество различных типов трофеев, заданных в массиве lootTypes на карте.
		unsigned int type;
		FloatCoord coord;
	};

	// лут с id
	struct LootWithId :Loot {
		unsigned long long id{ 0 };
	};

	/// @brief направление перемещения игрока
	enum class Direction { NORTH = 'U', SOUTH = 'D', WEST = 'L', EAST = 'R' };

	/// @brief Преобразователь энума Direction в строку для вывода в json
	/// @param direction направление
	std::string DirectionToString(Direction direction);

	class Road {
		struct HorizontalTag {
			explicit HorizontalTag() = default;
		};

		struct VerticalTag {
			explicit VerticalTag() = default;
		};

	public:
		// ширина дороги ROAD_OFFSET*2
		constexpr static double ROAD_OFFSET{ 0.4 };
		constexpr static HorizontalTag HORIZONTAL{};
		constexpr static VerticalTag VERTICAL{};

		Road(HorizontalTag, Point start, Coord end_x) noexcept
			: start_{ start }, end_{ end_x, start.y } {
			SetBorders();
		}

		Road(VerticalTag, Point start, Coord end_y) noexcept
			: start_{ start }, end_{ start.x, end_y } {
			SetBorders();
		}

		bool IsHorizontal() const noexcept { return start_.y == end_.y; }

		bool IsVertical() const noexcept { return start_.x == end_.x; }

		Point GetStart() const noexcept { return start_; }

		Point GetEnd() const noexcept { return end_; }

		/// @brief проверка нахождения движения игрока в границах дороги
		/// @param player_pos текущая координата игрока
		/// @param direction направление движения
		/// @return false - движение выходит за границы дороги, либо заданная
		/// координата вне дороги
		bool IsMoveInBorders(const FloatCoord& player_pos, Direction direction) const;

		/// @brief
		/// @param player_pos
		/// @param distance
		/// @param direction
		/// @return
		bool IsNextPositionOutOfBorders(const FloatCoord& player_pos, double distance,
			Direction direction);

		/// @brief
		/// @return
		RoadBorders GetBorders() const noexcept;

	private:
		/// @brief установка границ дороги с учётом её ширины
		void SetBorders();

	private:
		Point start_;
		Point end_;
		// границы дороги с учётом её ширины
		RoadBorders borders_;
	};

	class Building {
	public:
		explicit Building(Rectangle bounds) noexcept : bounds_{ bounds } {}

		const Rectangle& GetBounds() const noexcept { return bounds_; }

	private:
		Rectangle bounds_;
	};

	class Office {
	public:
		using Id = util::Tagged<std::string, Office>;

		Office(Id id, Point position, Offset offset) noexcept
			: id_{ std::move(id) }, position_{ position }, offset_{ offset } {}

		const Id& GetId() const noexcept { return id_; }

		Point GetPosition() const noexcept { return position_; }

		Offset GetOffset() const noexcept { return offset_; }

	private:
		Id id_;
		Point position_;
		Offset offset_;
	};

	class Map {
	public:
		// скорость игрока
		double dog_speed{ 0 };

		// Вместимость рюкзаков на карте
		std::optional<unsigned int> bag_capacity_;

		using Id = util::Tagged<std::string, Map>;
		using Roads = std::vector<Road>;
		using Buildings = std::vector<Building>;
		using Offices = std::vector<Office>;
		using LootTypes = std::vector<LootType>;

		Map(Id id, std::string name) noexcept
			: id_(std::move(id)), name_(std::move(name)) {}

		const Id& GetId() const noexcept { return id_; }

		const std::string& GetName() const noexcept { return name_; }

		const Buildings& GetBuildings() const noexcept { return buildings_; }

		const Roads& GetRoads() const noexcept { return roads_; }

		const Offices& GetOffices() const noexcept { return offices_; }

		void AddRoad(const Road& road) { roads_.emplace_back(road); }

		void AddBuilding(const Building& building) {
			buildings_.emplace_back(building);
		}

		void AddOffice(const Office& office);

		/// @brief Добавить тип лута
		/// @param loot_type тип лута
		void AddLootType(const LootType& loot_type);

		const LootTypes& GetLootTypes() const noexcept;

		unsigned int GetValueByLootType(unsigned int type) const;

		/// @brief Получить количество типов лута
		/// @return количество типов лута
		unsigned int GetLootTypesCount() const;

	private:
		using OfficeIdToIndex =
			std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

		Id id_;
		std::string name_;
		Roads roads_;
		Buildings buildings_;

		OfficeIdToIndex warehouse_id_to_index_;
		Offices offices_;
		LootTypes loot_types_;
	};

	class Player {
	public:
		// Координаты пса на карте задаются двумя вещественными числами. Для описания
		// вещественных координат разработайте структуру или класс.
		FloatCoord pos_;

		// Скорость пса на карте задаётся также двумя вещественными числами.Скорость
		// измеряется в единицах карты за одну секунду.
		FloatCoord speed_;

		// Направление в пространстве принимает одно из четырех значений :
		// NORTH(север), SOUTH(юг), WEST(запад), EAST(восток).
		Direction direction_{ Direction::NORTH };

		// id
		unsigned int id_{ 0 };

		std::shared_ptr<const Map> map_;

		std::string name_;

		std::list<LootWithId> bag_;

		// база
		FloatCoord base_pos_;

		// очки
		unsigned int score{ 0 };

	public:
		/// @brief перемещение игрока на заданную дистанцию
		/// @param distance дистанция
		void MoveOnDistance(double distance);

		/// @brief Перемещение игрока в заданном направлении до границы дороги и
		/// изменение значения дистанции для перещения
		/// @param road дорога по которой происходит перемещение игрока
		/// @param distance расстояние для перемещенияя
		void UpdatePosAndMoveDistanceByRoadBorders(const Road& road,
			double& distance);
	};

	class Game {
	public:
		// Ширина лута
		constexpr static double LOOT_WIDTH{ 0.0 };

		// Ширина игрока
		constexpr static double PLAYER_WIDTH{ 0.6 };

		// Ширина базы
		constexpr static double BASE_WIDTH{ 0.5 };

		Game() {};
		using Maps = std::vector<Map>;
		std::unordered_map<std::string, unsigned int> hash_to_palyer_id_;
		std::unordered_map<std::string, std::string> hash_to_map_name_;
		std::unordered_map<unsigned int, std::string> palyer_id_to_player_name_;

		std::mutex mtx_map_name_to_players_;
		std::unordered_map<std::string, std::deque<Player>> map_name_to_players_;

		std::mutex mtx_map_name_to_loot_;
		std::unordered_map<std::string, std::deque<Loot>> map_name_to_loot_;

		// генератор предметов
		std::optional<loot_gen::LootGenerator> loot_generator_;

		// Вместимость рюкзаков по-умолчанию
		unsigned int default_bag_capacity_{ 3 };

		double default_dog_speed{ 1.0 };
		bool world_auto_update{ false };
		bool random_start_pos{ false };

		/// @brief Просчёт игрового времени
		/// @param period_ms интервал просчитыаемого времени
		void SpendTime(std::chrono::milliseconds period_ms);

		void AddMap(Map map);

		const Maps& GetMaps() const noexcept { return maps_; }

		const Map* FindMap(const Map::Id& id) const noexcept {
			if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
				return &maps_.at(it->second);
			}
			return nullptr;
		}

		/// @brief установка пусти к статическим файлам
		/// @param static_path абсолютный путь к статическим файлам
		void SetStaticPath(const std::string& static_path);

		/// @brief получить доступ к пути к статическим файлам
		/// @return
		const std::string& GetStaticPath() const;

		/// @brief перемещение игрока
		/// @param direction направление перемещения
		/// @param token хэш
		/// @param map_name имя карты
		void MovePlayer(std::string direction, std::string& token,
			std::string& map_name);

		/// @brief получить список игроков на карте
		/// @param copy_players_on_map копия игроков на карте
		/// @param map_name имя карты
		void GetPlayersOnMap(std::deque<Player>& copy_players_on_map, const std::string& map_name);

		/// @brief получить список лута на карте
		/// @param copy_loot_on_map копия лута на карте
		/// @param map_name имя карты
		void GetLootOnMap(std::deque<Loot>& copy_loot_on_map, std::string map_name);

		/// @brief получить имя карты по хэшу
		/// @param token токен игрока
		/// @return nullptr такой карты нет
		const std::string* GetMapNameByHash(const std::string& token);

		/// @brief добавление игрока на карту
		int AddPlayerOnMap(const Map* map, const std::string& map_name,
			const std::string& hash, const std::string& user_name);

	private:
		using MapIdHasher = util::TaggedHasher<Map::Id>;
		using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

		std::vector<Map> maps_;
		MapIdToIndex map_id_to_index_;

		// путь к статическим файлам
		std::string static_path_;

		Provider provider_;
	};

}  // namespace model
