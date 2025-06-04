#include "model.h"

#include <assert.h>

#include <stdexcept>
#include <utility>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "random_functions.h"

namespace model {
	const static double EPS{ 0.00000001 };
	using namespace std::literals;

	void Map::AddOffice(const Office& office) {
		if (warehouse_id_to_index_.contains(office.GetId())) {
			throw std::invalid_argument("Duplicate warehouse");
		}

		const size_t index = offices_.size();
		Office& o = offices_.emplace_back(std::move(office));
		try {
			warehouse_id_to_index_.emplace(o.GetId(), index);
		}
		catch (...) {
			// Удаляем офис из вектора, если не удалось вставить в unordered_map
			offices_.pop_back();
			throw;
		}
	}

	void Map::AddLootType(const LootType& loot_type) {
		loot_types_.emplace_back(std::move(loot_type));
	}

	const Map::LootTypes& Map::GetLootTypes() const noexcept {
		return loot_types_;
	}

	uint64_t Map::GetValueByLootType(uint64_t type) const {
		return static_cast<uint64_t>(loot_types_.at(type).value);
	}

	uint64_t Map::GetLootTypesCount() const {
		return static_cast<uint64_t>(loot_types_.size());
	}

	void Map::SetDogSpeed(double dog_speed) {
		dog_speed_ = dog_speed;
	}

	void Map::SetBagCapacity(uint64_t bag_capcity) {
		bag_capacity_ = bag_capcity;
	}

	double Map::GetDogSpeed() const {
		return dog_speed_;
	}

	boost::optional<uint64_t> Map::GetBagCapacity() const {
		return bag_capacity_;
	}

	double int_to_double(int value) { return static_cast<double>(value); }

	void Road::SetBorders() {
		double start_x = static_cast<double>(start_.x);
		double start_y = static_cast<double>(start_.y);
		double end_x = static_cast<double>(end_.x);
		double end_y = static_cast<double>(end_.y);

		borders_.left_border = start_x - ROAD_OFFSET;
		borders_.right_border = start_x + ROAD_OFFSET;
		borders_.up_border = start_y - ROAD_OFFSET;
		borders_.down_border = start_y + ROAD_OFFSET;
		if (IsVertical()) {
			if (start_.y < end_.y) {
				borders_.up_border = start_y;
				borders_.down_border = end_y;
			} else {
				borders_.up_border = end_y;
				borders_.down_border = start_y + ROAD_OFFSET;
			}
			borders_.up_border -= Road::ROAD_OFFSET;
			borders_.down_border += Road::ROAD_OFFSET;
		}
		if (IsHorizontal()) {
			if (start_.x < end_.x) {
				borders_.left_border = start_x;
				borders_.right_border = end_x;
			} else {
				borders_.left_border = end_x;
				borders_.right_border = start_x;
			}
			borders_.left_border -= ROAD_OFFSET;
			borders_.right_border += ROAD_OFFSET;
		}
	}

	bool Road::IsMoveInBorders(const FloatCoord& player_pos,
		Direction direction) const {
		if (player_pos.x > borders_.right_border ||
			player_pos.x < borders_.left_border ||
			player_pos.y > borders_.down_border ||
			player_pos.y < borders_.up_border) {
			return false;
		}

		if (direction == Direction::EAST &&
			std::abs(player_pos.x - borders_.right_border) > EPS) {
			return true;
		} else if (direction == Direction::WEST &&
			std::abs(player_pos.x - borders_.left_border) > EPS) {
			return true;
		} else if (direction == Direction::SOUTH &&
			std::abs(player_pos.y - borders_.down_border) > EPS) {
			return true;
		} else if (direction == Direction::NORTH &&
			std::abs(player_pos.y - borders_.up_border) > EPS) {
			return true;
		}

		return false;
	}

	bool Road::IsNextPositionOutOfBorders(const FloatCoord& player_pos,
		double distance, Direction direction) {
		if (direction == Direction::EAST &&
			(player_pos.x + distance < borders_.right_border)) {
			return false;
		}
		if (direction == Direction::WEST &&
			(player_pos.x + distance > borders_.left_border)) {
			return false;
		} else if (direction == Direction::SOUTH &&
			(player_pos.y + distance < borders_.down_border)) {
			return false;
		} else if (direction == Direction::NORTH &&
			(player_pos.y + distance > borders_.up_border)) {
			return false;
		}
		return true;
	}

	RoadBorders Road::GetBorders() const noexcept {
		return borders_;
	}

	FloatCoord GetRandomPos(const Map* map) {
		auto roads{ map->GetRoads() };
		auto rundom_road_id = random_functions::RandomNumberFromZero(
			map->GetRoads().empty() ? 0
			: static_cast<int>(map->GetRoads().size() - 1));
		auto start = roads.at(rundom_road_id).GetStart();
		auto end = roads.at(rundom_road_id).GetEnd();
		auto start_x = start.x;
		auto end_x = end.x;
		if (start_x > end_x) {
			std::swap(start_x, end_x);
		}
		auto start_y = start.y;
		auto end_y = end.y;
		if (start_y > end_y) {
			std::swap(start_y, end_y);
		}
		auto random_x = random_functions::RandomDoubleNumber(start_x, end_x);
		auto random_y = random_functions::RandomDoubleNumber(start_y, end_y);
		return { random_x, random_y };
	}

	void Game::AddMap(Map map) {
		const size_t index = maps_.size();
		if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index);
			!inserted) {
			throw std::invalid_argument("Map with id "s + *map.GetId() +
				" already exists"s);
		} else {
			try {
				maps_.emplace_back(std::move(map));
			}
			catch (...) {
				map_id_to_index_.erase(it);
				throw;
			}
		}
	}

	void Game::SetStaticPath(const std::string& static_path) {
		static_path_ = static_path;
	}

	const std::string& Game::GetStaticPath() const {
		return static_path_;
	}

	/// @brief Поиск дороги по наличию места для передвижения в заданном направлении
	/// @param map карта с игроком
	/// @param player_pos текущая координата игрока
	/// @param direction направление движения
	/// @return дорога по которой можно выполнить перемещение
	boost::optional<Road> FindRoad(const Map& map, const FloatCoord& player_pos,
		Direction direction) {
		for (const auto& road : map.GetRoads()) {
			if (road.IsMoveInBorders(player_pos, direction)) {
				return road;
			}
		}
		return boost::none;
	}

	void Player::UpdatePosAndMoveDistanceByRoadBorders(const Road& road,
		double& distance) {
		RoadBorders road_borders{ road.GetBorders() };
		if (direction_ == Direction::EAST) {
			distance -= road_borders.right_border - pos_.x;
			pos_.x = road_borders.right_border;
		} else if (direction_ == Direction::WEST) {
			distance -= road_borders.left_border - pos_.x;
			pos_.x = road_borders.left_border;
		} else if (direction_ == Direction::SOUTH) {
			distance -= road_borders.down_border - pos_.y;
			pos_.y = road_borders.down_border;
		} else if (direction_ == Direction::NORTH) {
			distance -= road_borders.up_border - pos_.y;
			pos_.y = road_borders.up_border;
		}
	}

	void Player::MoveOnDistance(Game& game, double distance) {
		if (map_name_.empty()) {
			return;
		}

		if (std::abs(distance) < EPS) {
			return;
		}

		Map::Id map_id{ map_name_ };
		auto map_ptr = game.FindMap(map_id);
		boost::optional<Road> road{ FindRoad(*map_ptr, pos_, direction_) };

		// дорога не найдена
		if (!road.has_value()) {
			speed_.x = 0.0;
			speed_.y = 0.0;
			return;  // выход из рекрсии 1
		}

		// проверяем, что заданная дистания умещается в границы дороги
		if (road.value().IsNextPositionOutOfBorders(pos_, distance, direction_)) {
			UpdatePosAndMoveDistanceByRoadBorders(road.value(), distance);
			MoveOnDistance(game, distance);
		} else {  // выход из рекурсии 2
			if (direction_ == Direction::EAST || direction_ == Direction::WEST) {
				pos_.x += distance;
			} else {
				pos_.y += distance;
			}
		}
	}

	/// @brief проверяем прошёл ли игрок базу
	/// @param player игрок
	/// @param start_pos координата начала пути перещения
	/// @return false - база не пройдена
	bool IsBaseReached(const Player& player, FloatCoord start_pos) {
		collision_detector::Gatherer gatherer{ {start_pos.x, start_pos.y}, {player.pos_.x, player.pos_.y}, Game::PLAYER_WIDTH };
		// проверяем прошёл ли игрок базу
		Provider base_provider_;
		collision_detector::Item item{ { player.base_pos_.x, player.base_pos_.y }, Game::BASE_WIDTH };
		base_provider_.AddGatherer(gatherer);
		base_provider_.AddItem(item);
		// если игрок прошёл базу, то обновляем его счёт и опустошаем инвентарь
		auto base_reached = collision_detector::FindGatherEvents(base_provider_);
		return !base_reached.empty();
	}

	void Game::UpdatePlayerScore(Player& player) {
		Map::Id map_id{ player.map_name_ };
		auto map_ptr = FindMap(map_id);
		for (const auto& loot : player.bag_) {
			player.score_ += map_ptr->GetValueByLootType(loot.type);
		}
		player.bag_.clear();
	}

	/// @brief айдишники лута, которые нужно убрать с карты
	/// @param provider события подбора лута
	/// @param players игроки
	/// @param loot_on_map лут на карте
	/// @param current_bag_capacity вместимость инвентаря на карте
	/// @return айдишники
	std::deque<size_t> LootIdsToEraseFromMap(const Provider& provider, std::deque<Player> players, const std::deque<Loot>& loot_on_map, uint64_t current_bag_capacity) {
		std::deque<size_t> erased;
		if (players.empty()) {
			return erased;
		}

		auto events = collision_detector::FindGatherEvents(provider);

		for (const auto& current_event : events) {
			if (std::find(erased.begin(), erased.end(), current_event.item_id) == erased.end() &&
				players.at(current_event.gatherer_id).bag_.size() < current_bag_capacity) {
				players[current_event.gatherer_id].bag_.emplace_back(loot_on_map.at(current_event.item_id), current_event.item_id);
				erased.emplace_back(current_event.item_id);
			}
		}
		return erased;
	}

	/// @brief удаление подобранного лута с карты
	/// @param provider события подбора лута
	/// @param erased список id удалённого лута
	/// @param loot_on_map лут на карте
	void EraseLootFromMap(Provider& provider, std::deque<size_t> erased, std::deque<Loot>& loot_on_map) {
		auto it = loot_on_map.begin();
		unsigned int index{ 0 };
		unsigned int minus{ 1 };

		if (!erased.empty()) {// защита удаления

			auto is_even = [&](const Loot& loot) {

				if (erased.empty()) {
					return false;
				}

				if (index == *erased.begin()) {
					erased.erase(erased.begin());
					if (erased.empty()) {
						return true;
					}
					*erased.begin() -= minus;
					++minus;
					return true;
				} else {
					++index;
				}
				return false;
			};
			loot_on_map.erase(std::remove_if(loot_on_map.begin(), loot_on_map.end(), is_even), loot_on_map.end());
			provider.ClearItems();
			for (const auto& current_loot : loot_on_map) {
				collision_detector::Item item{ { current_loot.coord.x, current_loot.coord.y }, Game::LOOT_WIDTH };
				provider.AddItem(item);
			}
		}
	}

	/// @brief генерация лута на карте
	void GenerateLoot(boost::optional<loot_gen::LootGenerator>& loot_generator,
		std::chrono::milliseconds period_ms,
		std::deque<Loot>& loot_on_map,
		unsigned int players_count,
		const Game& game,
		const std::string& map_name,
		Provider& provider) {
		auto loot_count_to_generate = loot_generator.value().Generate(period_ms,
			static_cast<unsigned int>(loot_on_map.size()),
			players_count);
		if (loot_count_to_generate > 0) {
			Map::Id map_id{ map_name };
			auto map_ptr = game.FindMap(map_id);
			auto loot_types_count = map_ptr->GetLootTypesCount();
			// Добавляем на карту лут только в том случае, если заданы тип для данной карты
			if (loot_types_count) {
				auto max_type_id = loot_types_count - 1;
				loot_on_map.emplace_back(random_functions::RandomNumberFromZero(max_type_id), GetRandomPos(map_ptr));
				collision_detector::Item item{ { loot_on_map.begin()->coord.x, loot_on_map.begin()->coord.y }, Game::LOOT_WIDTH };
				provider.AddItem(item);
			}
		}
	}

	void Game::SetDefaultDogSpeed(double default_dog_speed) {
		default_dog_speed_ = default_dog_speed;
	}

	void Game::SetLootGenerator(loot_gen::LootGenerator&& loot_generator) {
		loot_generator_ = std::move(loot_generator);
	}

	void Game::SetDefaultBagCapacity(uint64_t default_bag_capacity) {
		default_bag_capacity_ = default_bag_capacity;
	}

	void Game::SetRandomStartPosOn() {
		random_start_pos_ = true;
	}

	bool Game::IsWorldAutoUpdate() {
		return world_auto_update_;
	}

	double Game::GetDefaultDogSpeed() {
		return default_dog_speed_;
	}

	void Game::CopyGame(GameRepr& game_repr) {
		for (const auto& value : hash_to_palyer_id_) {
			game_repr.hash_to_palyer_id[value.first] = value.second;
		}

		for (const auto& value : hash_to_map_name_) {
			game_repr.hash_to_map_name[value.first] = value.second;
		}

		for (const auto& value : palyer_id_to_player_name_) {
			game_repr.palyer_id_to_player_name[value.first] = value.second;
		}

		for (const auto& value : map_name_to_players_) {
			auto& map_name_to_players = game_repr.map_name_to_players[value.first];
			std::copy(value.second.begin(), value.second.end(), inserter(map_name_to_players, map_name_to_players.begin()));
		}

		for (const auto& value : map_name_to_loot_) {
			auto& map_name_to_loot = game_repr.map_name_to_loot[value.first];
			std::copy(value.second.begin(), value.second.end(), inserter(map_name_to_loot, map_name_to_loot.begin()));
		}
	}

	void Game::LoadGame(const GameRepr& game_repr) {
		std::lock_guard<std::mutex> guard(mtx_map_name_to_players_);
		std::lock_guard<std::mutex> guard2(mtx_map_name_to_loot_);

		for (const auto& value : game_repr.hash_to_palyer_id) {
			hash_to_palyer_id_[value.first] = value.second;
		}

		for (const auto& value : game_repr.hash_to_map_name) {
			hash_to_map_name_[value.first] = value.second;
		}

		for (const auto& value : game_repr.palyer_id_to_player_name) {
			palyer_id_to_player_name_[value.first] = value.second;
		}

		for (const auto& value : game_repr.map_name_to_players) {
			auto& players = map_name_to_players_[value.first];
			std::copy(value.second.begin(), value.second.end(), inserter(players, players.begin()));
		}

		for (const auto& value : game_repr.map_name_to_loot) {
			auto& loot = map_name_to_loot_[value.first];
			std::copy(value.second.begin(), value.second.end(), inserter(loot, loot.begin()));
		}
	}

	void Game::SetStateFilePath(std::string state_file_path) {
#ifdef WIN32
		std::replace(state_file_path.begin(), state_file_path.end(), '/', '\\');
#endif
		state_file_path_ = state_file_path;
	}

	void Game::SerilizeState() {

		if (!state_file_path_.has_value()) {
			return;
		}

		std::string temp_state_file_path(state_file_path_.value());
		std::string original_state_file_path(state_file_path_.value());
		temp_state_file_path.append("_tmp");

		std::ofstream state_file{ temp_state_file_path }; // проверить, что в конце есть доп слеши
		if (!state_file) {
			throw std::invalid_argument("Open to state file to save error");
		}

		std::stringstream ss;
		boost::archive::text_oarchive oa{ ss };
		GameRepr game_repr;
		try {
			CopyGame(game_repr);
		}
		catch (...) {
			throw std::invalid_argument("Wtf error");
		}

		oa << game_repr;
		state_file << ss.rdbuf();
		state_file.flush();
		state_file.close();
		try {
			std::filesystem::rename(temp_state_file_path, original_state_file_path);
		}
		catch (...) {
			throw std::invalid_argument("Rename error");
		}

	}

	void Game::DeserilizeState() {
		if (!state_file_path_.has_value()) {
			return;
		}

		std::ifstream state_file{ state_file_path_.value() }; // проверить, что в конце есть доп слеши
		if (!state_file) {
			// Когда сервер запускается с указанием пути к отсутствующему файлу состояния, он должен стартовать с чистого листа. 
			return;
		}
		std::stringstream ss;
		ss << state_file.rdbuf();
		boost::archive::text_iarchive ia{ ss };
		GameRepr game_repr;
		try {
			ia >> game_repr;
			LoadGame(game_repr);
		}
		catch (...) {
			throw std::invalid_argument("Deserialization error");
		}
	}

	void Game::SetSaveStatePeriod(std::chrono::milliseconds save_state_period_ms) {
		save_state_period_ms_ = save_state_period_ms;
	}
	void Game::SetDogRetirementTime(double dog_retirement_time) {
		dog_retirement_time_ = dog_retirement_time;
	}

	void Game::WriteDataToDB(const std::vector<postgres::RetiredPlayer>& left_players) {
		if (!left_players.empty()) {
			auto connect_db = connection_pool_.GetConnection();
			postgres::WriteRetiredToDatabase(*connect_db, left_players);
		}
	}

	void Game::SpendTime(std::chrono::milliseconds period_ms) {
		std::lock_guard<std::mutex> guard(mtx_map_name_to_players_);
		std::lock_guard<std::mutex> guard2(mtx_map_name_to_loot_);
		std::lock_guard<std::mutex> guard3(mtx_hash_to_map_name_);

		auto to_sec = [](std::chrono::milliseconds _period_ms) {
			const double MS_TO_SEC = 1000.0;
			return static_cast<double>(_period_ms.count()) / MS_TO_SEC; };
		double delta_time = to_sec(period_ms);


		auto new_time = current_game_time_ + delta_time;
		for (auto& map_palyers : map_name_to_players_) {
			std::vector<postgres::RetiredPlayer> left_players;
			for (auto& player : map_palyers.second) {
				if (!player.join_time_.has_value()) {
					player.join_time_ = current_game_time_;
				}
				// Перемещение текущего игрока по дороге текущей карты
				// Дистанция на которую нужно выполнить перемещение
				double distance{ 0.0 };
				// игрок покинул игру, дальше идти смысла нет
				if (player.is_left_game_) {
					continue;
				}
				if (std::abs(player.speed_.x) < EPS && std::abs(player.speed_.y) < EPS) {
					auto to_ms = [](double _period_s) {
						const double SEC_TO_MS = 1000.0;
						return static_cast<int>(_period_s * SEC_TO_MS); };
					player.no_move_time_ += delta_time;
					if (to_ms(player.no_move_time_) >= to_ms(dog_retirement_time_)) {
						player.is_left_game_ = true;
						invalid_tokens_.emplace_back(player.hash_);
						hash_to_map_name_.erase(player.hash_);
						left_players.emplace_back(static_cast<int>(player.id_), player.name_,
							static_cast<int>(player.score_),
							static_cast<int>(new_time - player.join_time_.value()));
					}
					continue;
				}
				player.no_move_time_ = 0.0;
				double delta_time_float = static_cast<double>(delta_time);
				if (player.direction_ == Direction::EAST ||
					player.direction_ == Direction::WEST) {
					distance = player.speed_.x * delta_time_float;
				} else {
					distance = player.speed_.y * delta_time_float;
				}
				// позиция до начала перемещения
				FloatCoord start_pos{ player.pos_ };
				//вход в рекурсию
				player.MoveOnDistance(*this, distance);

				// Добавляем игрока к списку сборщиков лута
				collision_detector::Gatherer gatherer{ {start_pos.x, start_pos.y}, {player.pos_.x, player.pos_.y}, PLAYER_WIDTH };
				provider_.AddGatherer(gatherer);

				// проверяем прошёл ли игрок базу
				if (IsBaseReached(player, start_pos)) {
					UpdatePlayerScore(player);
				}
			}
			current_game_time_ = new_time;
			WriteDataToDB(left_players);

			// формируем список лута под удаление с карты
			auto& loot_on_map = map_name_to_loot_[map_palyers.first];
			Map::Id map_id{ map_palyers.second.at(0).map_name_ };
			auto map_ptr = FindMap(map_id);
			auto map_bag_capacity = map_ptr->GetBagCapacity();
			auto current_bag_capacity = map_bag_capacity.has_value() ? map_bag_capacity.value() : default_bag_capacity_;
			auto erased = LootIdsToEraseFromMap(provider_, map_palyers.second, loot_on_map, current_bag_capacity);

			// удаление лута с карты
			EraseLootFromMap(provider_, erased, loot_on_map);

			provider_.ClearGatherers();

			// генерация лута
			if (loot_generator_.has_value()) {
				GenerateLoot(loot_generator_, period_ms, loot_on_map, static_cast<unsigned int>(map_palyers.second.size()), *this, map_palyers.first, provider_);
			}
		}

		static double last_update_time{ 0.0 };
		if (save_state_period_ms_.has_value()) {
			last_update_time += delta_time;
			double save_state_period_sec = to_sec(save_state_period_ms_.value());
			if (last_update_time > save_state_period_sec) {
				SerilizeState();
				last_update_time = 0.0;
			}
		}

	}

	void Game::MovePlayer(std::string direction, std::string& token,
		std::string& map_name) {
		auto players_on_map = map_name_to_players_.find(map_name);
		if (players_on_map == map_name_to_players_.end()) {
			return;
		}

		auto player_id = hash_to_palyer_id_.find(token);
		if (player_id == hash_to_palyer_id_.end()) {
			return;
		}

		auto player = std::find_if(
			players_on_map->second.begin(), players_on_map->second.end(),
			[&](const Player& player) { return player.id_ == player_id->second; });
		if (player == players_on_map->second.end()) {
			return;
		}

		Map::Id map_id{ player->map_name_ };
		auto map_ptr = FindMap(map_id);
		if (direction == "L") {
			player->direction_ = model::Direction::WEST;
			player->speed_.x = -map_ptr->GetDogSpeed();
			player->speed_.y = 0.0;
		} else if (direction == "R") {
			player->direction_ = model::Direction::EAST;
			player->speed_.x = map_ptr->GetDogSpeed();
			player->speed_.y = 0.0;
		} else if (direction == "U") {
			player->direction_ = model::Direction::NORTH;
			player->speed_.x = 0.0;
			player->speed_.y = -map_ptr->GetDogSpeed();
		} else if (direction == "D") {
			player->direction_ = model::Direction::SOUTH;
			player->speed_.x = 0.0;
			player->speed_.y = map_ptr->GetDogSpeed();
		} else if (direction == "") {
			player->speed_.x = 0.0;
			player->speed_.y = 0.0;
		} else {
			assert(false);
		};
	}

	void Game::GetPlayersOnMap(std::deque<Player>& copy_players_on_map, const std::string& map_name) {
		std::lock_guard<std::mutex> guard(mtx_map_name_to_players_);
		copy_players_on_map.clear();
		auto players_on_map = map_name_to_players_.find(map_name); //TODO не npos
		std::copy(players_on_map->second.begin(), players_on_map->second.end(), inserter(copy_players_on_map, copy_players_on_map.begin()));
	}

	void Game::GetLootOnMap(std::deque<Loot>& copy_loot_on_map, std::string map_name) {
		std::lock_guard<std::mutex> guard(mtx_map_name_to_loot_);
		copy_loot_on_map.clear();
		auto loot_on_map = map_name_to_loot_.find(map_name); //TODO не npos
		if (loot_on_map == map_name_to_loot_.end()) {
			return;
		}
		std::copy(loot_on_map->second.begin(), loot_on_map->second.end(), inserter(copy_loot_on_map, copy_loot_on_map.begin()));
	}

	const std::string* Game::GetMapNameByHash(const std::string& token) {
		std::lock_guard<std::mutex> guard(mtx_hash_to_map_name_);
		auto invalid_token = std::find(invalid_tokens_.begin(), invalid_tokens_.end(), token);
		if (invalid_token != invalid_tokens_.end()) {
			return nullptr;
		}
		auto map_name = hash_to_map_name_.find(token);
		if (map_name == hash_to_map_name_.end()) {
			return nullptr;
		}
		return &map_name->second;
	}

	int Game::AddPlayerOnMap(const Map* map, const std::string& map_name,
		const std::string& hash,
		const std::string& user_name) {

		int new_id = static_cast<int>(hash_to_palyer_id_.size() + 1);// std::stoi(util::detail::UUIDToString(util::detail::NewUUID()));//

		hash_to_palyer_id_[hash] = new_id;
		hash_to_map_name_[hash] = map_name;
		palyer_id_to_player_name_[new_id] = user_name;

		// После добавления на карту пёс должен иметь имеет скорость, равную нулю.
		// Координаты пса — случайно выбранная точка на случайно выбранном отрезке
		// дороги этой карты. Направление пса по умолчанию — на север.
		model::Player new_player;
		if (random_start_pos_) {
			new_player.pos_ = GetRandomPos(map);
		} else {
			auto roads{ map->GetRoads() };
			new_player.pos_ = { static_cast<double>(roads.begin()->GetStart().x),
							   static_cast<double>(roads.begin()->GetStart().y) };
		}
		//new_player.map_ = std::shared_ptr<const model::Map>(map);
		new_player.map_name_ = *map->GetId();
		new_player.id_ = new_id;
		new_player.name_ = user_name;
		new_player.hash_ = hash;
		new_player.base_pos_ = new_player.pos_;
		map_name_to_players_[map_name].emplace_back(std::move(new_player));
		return new_id;
	}

	std::string DirectionToString(Direction direction) {
		if (direction == Direction::NORTH) {
			return "U"s;  // Up
		} else if (direction == Direction::SOUTH) {
			return "D";  // Down
		} else if (direction == Direction::WEST) {
			return "L";  // Left
		} else if (direction == Direction::EAST) {
			return "R";  // Right
		}
		return {};
	}
}  // namespace model
