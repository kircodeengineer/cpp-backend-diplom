#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/result>
#include <pqxx/pqxx>
#include <mutex>
#include <condition_variable>
#include <vector>


namespace postgres {
	// Игрок покинувший игру
	struct RetiredPlayer {
		int id;
		std::string name;
		int score;
		int play_time_s;
	};

	/// @brief создать таблицу
	/// @param conn соединение с БД
	void CreateTable(pqxx::connection& conn);

	/// @brief записать покинвших игру игроков в БД
	/// @param conn соединение с БД
	/// @param retired_players покинвшие игру игроки
	void WriteRetiredToDatabase(pqxx::connection& conn, const std::vector<RetiredPlayer>& retired_players);

	/// @brief чтение из БД таблицы с покинувших игру игроков
	/// @param conn соединение с БД
	/// @param start целое число, задающее номер начального элемента (0 — начальный элемент)
	/// @param max_items целое число, задающее максимальное количество элементов
	/// @param retired_players 
	void ReadRetiredFromDatabase(pqxx::connection& conn, int start, int max_items, std::vector<RetiredPlayer>& retired_players);


	class ConnectionPool {
		using PoolType = ConnectionPool;
		using ConnectionPtr = std::shared_ptr<pqxx::connection>;

	public:
		class ConnectionWrapper {
		public:
			ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
				: conn_{ std::move(conn) }
				, pool_{ &pool } {
			}

			ConnectionWrapper(const ConnectionWrapper&) = delete;
			ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

			ConnectionWrapper(ConnectionWrapper&&) = default;
			ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

			pqxx::connection& operator*() const& noexcept {
				return *conn_;
			}
			pqxx::connection& operator*() const&& = delete;

			pqxx::connection* operator->() const& noexcept {
				return conn_.get();
			}

			~ConnectionWrapper() {
				if (conn_) {
					pool_->ReturnConnection(std::move(conn_));
				}
			}

		private:
			std::shared_ptr<pqxx::connection> conn_;
			PoolType* pool_;
		};

		// ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
		template <typename ConnectionFactory>
		ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
			pool_.reserve(capacity);
			for (size_t i = 0; i < capacity; ++i) {
				pool_.emplace_back(connection_factory());
			}
		}

		ConnectionWrapper GetConnection() {
			std::unique_lock lock{ mutex_ };
			// Блокируем текущий поток и ждём, пока cond_var_ не получит уведомление и не освободится
			// хотя бы одно соединение
			cond_var_.wait(lock, [this] {
				return used_connections_ < pool_.size();
				});
			// После выхода из цикла ожидания мьютекс остаётся захваченным

			return { std::move(pool_[used_connections_++]), *this };
		}

	private:
		void ReturnConnection(ConnectionPtr&& conn) {
			// Возвращаем соединение обратно в пул
			{
				std::lock_guard lock{ mutex_ };
				assert(used_connections_ != 0);
				pool_[--used_connections_] = std::move(conn);
			}
			// Уведомляем один из ожидающих потоков об изменении состояния пула
			cond_var_.notify_one();
		}

		std::mutex mutex_;
		std::condition_variable cond_var_;
		std::vector<ConnectionPtr> pool_;
		size_t used_connections_ = 0;
	};

}  // namespace postgres
