#pragma once
#include <deque>
#include <list>
#include <memory>
#include <boost/serialization/optional.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "loot_generator.h"
#include "tagged.h"
#include "collision_detector.h"
#include "postgres.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/list.hpp>

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

		template<class Archive>
		void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
			ar& x;
			ar& y;
		}
	};



	// описание типа лута по config.json
	struct LootType {
		std::string_view name{ LootTypesNames::KEY };
		std::string_view file{ LootTypesFiles::KEY };
		std::string_view type{ MapObjTypes::OBJ };
		boost::optional<uint64_t> rotation;
		boost::optional<std::string> color;
		double scale{ 0.0 };
		uint64_t value{ 0 };
	};

	// лут на карте
	struct Loot {
		// целое число, задающее тип объекта в диапазоне [0, N-1], где N — количество различных типов трофеев, заданных в массиве lootTypes на карте.
		uint64_t type;
		FloatCoord coord;

		template<class Archive>
		void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
			ar& type;
			ar& coord;
		}
	};

	// лут с id
	struct LootWithId : Loot {
		uint64_t id{ 0 };

		template<class Archive>
		void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
			ar& id;
		}
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

		/// @brief 
		/// @return 
		const LootTypes& GetLootTypes() const noexcept;

		/// @brief 
		/// @param type 
		/// @return 
		uint64_t GetValueByLootType(uint64_t type) const;

		/// @brief Получить количество типов лута
		/// @return количество типов лута
		uint64_t GetLootTypesCount() const;

		/// @brief установить скорость игрока
		/// @param dog_speed скорость игрока
		void SetDogSpeed(double dog_speed);

		/// @brief установить вместимость рюкзака
		/// @param bag_capcity вместимость рюкзака
		void SetBagCapacity(uint64_t bag_capcity);

		/// @brief получить значения скорости игрока
		/// @return скорость игрока
		double GetDogSpeed() const;

		/// @brief получить вместимость рюкзака на карте
		/// @return вместимость рюкзака на карте
		boost::optional<uint64_t> GetBagCapacity() const;

	private:
		using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

		Id id_;
		std::string name_;
		Roads roads_;
		Buildings buildings_;

		OfficeIdToIndex warehouse_id_to_index_;
		Offices offices_;
		LootTypes loot_types_;

		// скорость игрока
		double dog_speed_{ 0 };

		// Вместимость рюкзаков на карте
		boost::optional<uint64_t> bag_capacity_;
	};

	class Game;

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
		uint64_t id_{ 0 };

		std::string map_name_;

		std::string name_;

		std::string hash_;

		std::list<LootWithId> bag_;

		// база
		FloatCoord base_pos_;

		// очки
		uint64_t score_{ 0 };

		// нет движения
		double no_move_time_{ 0.0 };

		// Покинул ли игрок игру
		bool is_left_game_{ false };

		// время входа в игру
		boost::optional<double> join_time_;
	public:
		/// @brief перемещение игрока на заданную дистанцию
		/// @param distance дистанция
		void MoveOnDistance(Game& game, double distance);

		/// @brief Перемещение игрока в заданном направлении до границы дороги и
		/// изменение значения дистанции для перещения
		/// @param road дорога по которой происходит перемещение игрока
		/// @param distance расстояние для перемещенияя
		void UpdatePosAndMoveDistanceByRoadBorders(const Road& road,
			double& distance);

		template<class Archive>
		void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
			ar& pos_;
			ar& speed_;
			ar& direction_;
			ar& id_;
			ar& map_name_;
			ar& name_;
			ar& hash_;
			ar& bag_;
			ar& base_pos_;
			ar& score_;
			ar& is_left_game_;
		}
	};

	struct GameRepr {
		std::unordered_map<std::string, uint64_t> hash_to_palyer_id;
		std::unordered_map<std::string, std::string> hash_to_map_name;
		std::unordered_map<uint64_t, std::string> palyer_id_to_player_name;
		std::unordered_map<std::string, std::deque<Player>> map_name_to_players;
		std::unordered_map<std::string, std::deque<Loot>> map_name_to_loot;

		template<class Archive>
		void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
			ar& hash_to_palyer_id;
			ar& hash_to_map_name;
			ar& palyer_id_to_player_name;
			ar& map_name_to_players;
			ar& map_name_to_loot;
		}
	};

	class Game {
	public:
		// Ширина лута
		constexpr static double LOOT_WIDTH{ 0.0 };

		// Ширина игрока
		constexpr static double PLAYER_WIDTH{ 0.6 };

		// Ширина базы
		constexpr static double BASE_WIDTH{ 0.5 };

		using Maps = std::vector<Map>;

		Game(postgres::ConnectionPool& connection_pool) : connection_pool_(connection_pool) {
			try {
				auto connection = connection_pool_.GetConnection();
				postgres::CreateTable(*connection);
			}
			catch (...)
			{
				throw std::invalid_argument("DataBase init error!");
			}
		};

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

		/// @brief установка значения скорости игрока по-умолчанию
		/// @param default_bag_capacity скорость игрока по-умолчанию
		void SetDefaultDogSpeed(double default_dog_speed);

		/// @brief получить скорость игрока по-умолчанию
		/// @return скорость игрока по-умолчанию
		double GetDefaultDogSpeed();

		/// @brief установка генератора лута
		/// @param loot_generator генератор лута
		void SetLootGenerator(loot_gen::LootGenerator&& loot_generator);

		/// @brief установить значение вместимости рюкзака по-умолчанию
		/// @param default_bag_capacity значение вместимости рюкзака по-умолчанию
		void SetDefaultBagCapacity(uint64_t default_bag_capacity);

		/// @brief установить выставку рандомной стартовой позиции
		void SetRandomStartPosOn();

		/// @brief выполняется ли автообновление мира?
		/// @return false - нет
		bool IsWorldAutoUpdate();

		/// @brief сериализация состояния игры
		void SerilizeState();

		/// @brief десериализация состояния игры
		void DeserilizeState();

		/// @brief копирование состояния игры под сериализацию
		/// @param game_repr состояние игры
		void CopyGame(GameRepr& game_repr);

		/// @brief загрузка состония игры из файла
		/// @param game_repr состояние игры из файла
		void LoadGame(const GameRepr& game_repr);

		/// @brief установка пути к файлу с состоянием игры
		/// @param state_file_path путь к файлу с состоянием игры
		void SetStateFilePath(std::string state_file_path);

		/// @brief установить период автоматической записи в файл состояния игры 
		/// @param save_state_period_ms_ период автоматической записи в файл состояния игры
		void SetSaveStatePeriod(std::chrono::milliseconds save_state_period_ms);

		/// @brief обновляем счёт игрока
		/// @param player игрок 
		void UpdatePlayerScore(Player& player);

		/// @brief установить время бездействия
		/// @param dog_retirement_time время бездействия
		void SetDogRetirementTime(double dog_retirement_time);

		/// @brief Запись данных о выбывших игроках в БД
		/// @param left_players выбывшие игроках в БД
		void WriteDataToDB(const std::vector<postgres::RetiredPlayer>& left_players);
	private:
		using MapIdHasher = util::TaggedHasher<Map::Id>;
		using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

		std::vector<Map> maps_;
		MapIdToIndex map_id_to_index_;

		// путь к статическим файлам
		std::string static_path_;

		// путь к статическим файлам
		boost::optional<std::string> state_file_path_;

		// коллайдер
		Provider provider_;

		std::unordered_map<std::string, uint64_t> hash_to_palyer_id_;
		std::mutex mtx_hash_to_map_name_;
		std::unordered_map<std::string, std::string> hash_to_map_name_;
		std::unordered_map<uint64_t, std::string> palyer_id_to_player_name_;

		std::mutex mtx_map_name_to_players_;
		std::unordered_map<std::string, std::deque<Player>> map_name_to_players_;

		std::mutex mtx_map_name_to_loot_;
		std::unordered_map<std::string, std::deque<Loot>> map_name_to_loot_;

		// генератор предметов
		boost::optional<loot_gen::LootGenerator> loot_generator_;

		// Вместимость рюкзаков по-умолчанию
		uint64_t default_bag_capacity_{ 3 };

		// Скорость игрока по-умолчанию
		double default_dog_speed_{ 1.0 };

		// флаг автоматического обновления состояния игры
		bool world_auto_update_{ false };

		// флаг рандомной стартовой позиции
		bool random_start_pos_{ false };

		// период автоматической записи в файл состояния игры 
		boost::optional<std::chrono::milliseconds> save_state_period_ms_;

		// пул потоков для работы с БД
		postgres::ConnectionPool& connection_pool_;

		// Время бездействия по достижению которого будет сделана запись в БД
		double dog_retirement_time_{ 60.0 };

		// Текущее игровое время, сек
		double current_game_time_{ 0.0 };

		// токены покинувших игру игроков
		std::deque<std::string> invalid_tokens_;
	};

}  // namespace model
