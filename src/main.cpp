#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/program_options.hpp>
#include <pqxx/pqxx>
#include <iostream>
#include <optional>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "ticker.h"
#include "postgres.h"


using namespace std::literals;
namespace net = boost::asio;
using namespace boost::json;
using namespace boost::posix_time;

namespace {
	// Cтруктура, которая будет хранить параметры приложения.
	struct Args {
		std::string tick_period;
		bool tick_period_exist{ false };
		std::string save_state_period;
		bool save_state_period_exist{ false };
		std::string config_file_path;
		std::string static_files_path;
		std::string state_file_path;
		bool state_file_exist{ false };
		bool random_spawn{ false };

	};

	/// @brief Парсинг командной строки разместим в функции ParseCommandLine.
	// Она, как и функция main, будет принимать параметры argc и argv.
	[[nodiscard]] std::optional<Args> ParseCommandLine(int argc,
		const char* const argv[]) {
		// Чтобы пользоваться библиотекой было удобно, зададим краткое название po для
		// пространства имён boost::program_options.
		namespace po = boost::program_options;

		// Сначала опишем опции программы.
		// Для этого воспользуемся классом options_description.
		// Эта коллекция хранит объекты типа option_description, каждый из которых
		// отвечает за параметр программы.
		po::options_description desc{ "Allowed options"s };

		// Обычно программа выводит справку о своём использовании только тогда, когда
		// запущена с одним из параметров командной строки: --help или -h. Чтобы
		// добавить эти параметры, используем метод options_description::add_options:
		Args args;
		desc.add_options()
			// Опция --help выводит информацию о параметрах командной строки.
			("help,h", "produce help message")
			// Опция --tick-period milliseconds задаёт период автоматического обновления игрового состояния в миллисекундах
			("tick-period,t",
				po::value(&args.tick_period)->multitoken()->value_name("milliseconds"s),
				"set tick period")
			// Опция --save-state-period milliseconds задаёт период автоматического сохранения состояния сервера
			("save-state-period,sv", po::value(&args.save_state_period)->multitoken()->value_name("state milliseconds"s),
				"set save state period")
			// Опция --config-file задаёт путь к конфигурационному JSON-файлу игры
			("config-file,c", po::value(&args.config_file_path)->value_name("file"s),
				"set config file path")
			// Опция --www-root задаёт путь к каталогу со статическими файлами игры
			("www-root,w", po::value(&args.static_files_path)->value_name("dir"s),
				"set static files root")
			// Опция --state-file задаёт путь к файлу, в который приложение должно сохранять своё состояние в процессе работы, а при старте — восстанавливать.
			("state-file,st", po::value(&args.state_file_path)->value_name("state file"s),
				"set state file path")
			// Опция randomize-spawn-points включает режим, при котором пёс игрока появляется в случайной точке случайно выбранной дороги карты
			("randomize-spawn-points", "spawn dogs at random positions");

		// variables_map хранит значения опций после разбора
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		// Класс variables_map унаследован от std::map, поэтому с ним можно работать,
		// используя привычные методы map.

		// Если встретим параметр --help — выведем справку.
		if (vm.contains("help"s)) {
			// // Если был указан параметр --help, то выводим справку и возвращаем
			// nullopt
			std::cout << desc;
			return std::nullopt;
		}

		if (vm.contains("randomize-spawn-points"s)) {
			args.random_spawn = true;
		}

		if (vm.contains("tick-period"s)) {
			args.tick_period_exist = true;
		}

		if (vm.contains("save-state-period"s)) {
			args.save_state_period_exist = true;
		}

		if (vm.contains("state-file"s)) {
			args.state_file_exist = true;
		}

		if (!vm.contains("config-file"s)) {
			throw std::runtime_error("Config file path is not specified"s);
		}

		if (!vm.contains("www-root"s)) {
			throw std::runtime_error("Static files root is not specified"s);
		}

		// С опциями программы всё в порядке, возвращаем структуру args
		return args;
	}

	/// @brief Запускает функцию fn на n потоках, включая текущий
	/// @tparam Fn тип возвращаемого значения функии
	/// @param threads_count число потоков
	/// @param fn функция для запуска в потоках
	template <typename Fn>
	void RunWorkers(unsigned threads_count, const Fn& fn) {
		threads_count = std::max(1u, threads_count);
		std::vector<std::jthread> workers;
		workers.reserve(threads_count - 1);
		// Запускаем n-1 рабочих потоков, выполняющих функцию fn
		while (--threads_count) {
			workers.emplace_back(fn);
		}
		fn();
	}

}  // namespace

/// @brief логгирование запуска сервера
/// @param port
/// @param adress
void LogStartServer(int port, std::string& adress) {
	object obj;
	obj[std::string(logger::Literals::TIMESTAMP)] =
		to_iso_extended_string(microsec_clock::universal_time());
	obj[std::string(logger::Literals::DATA)] = {
		{std::string(logger::Literals::PORT), port},
		{std::string(logger::Literals::ADDRESS), adress} };
	obj[std::string(logger::Literals::MESSAGE)] = "server started"s;
	LOG(serialize(obj));
}

/// @brief логгирование остановки сервера
/// @param port
/// @param adress
void LogExitServer(int code, std::optional<std::string> exception) {
	object obj;
	obj[std::string(logger::Literals::TIMESTAMP)] =
		to_iso_extended_string(microsec_clock::universal_time());
	if (exception == std::nullopt) {
		obj[std::string(logger::Literals::DATA)] = {
			{std::string(logger::Literals::CODE), code} };
	} else {
		obj[std::string(logger::Literals::DATA)] = {
			{std::string(logger::Literals::CODE), code},
			{std::string(logger::Literals::EXCEPTION), exception.value()} };
	}

	obj[std::string(logger::Literals::MESSAGE)] = "server exited"s;
	LOG(serialize(obj));
}

int main(int argc, const char* argv[]) {
	try {
		auto args = ParseCommandLine(argc, argv);

		const char* db_url = std::getenv("GAME_DB_URL");
		if (!db_url) {
			throw std::runtime_error("GAME_DB_URL is not specified");
		}

		postgres::ConnectionPool connection_pool{ 12, [db_url] { //10
									 auto conn = std::make_shared<pqxx::connection>(db_url);
									 return conn;
								 } };

		// 1. Загружаем карту из файла и построить модель игры
		model::Game game(connection_pool);

		json_loader::LoadGame(game, args->config_file_path);

		if (args->random_spawn) {
			game.SetRandomStartPosOn();
		}

		// Когда сервер запускается без указания пути к файлу с сохранённым состоянием, он должен стартовать с чистого листа. 
		if (args->state_file_exist) {
			game.SetStateFilePath(std::string(args->state_file_path));
			// Когда сервер запускается с указанием пути к существующему файлу состояния, он должен должен восстановить это состояние. 
			game.DeserilizeState();
		}


		if (args->save_state_period_exist) {
			game.SetSaveStatePeriod(static_cast<std::chrono::milliseconds>(std::stoi(args->save_state_period)));
		}

		game.SetStaticPath(std::string(args->static_files_path));

		// 2. Инициализируем io_context
		const unsigned num_threads = std::thread::hardware_concurrency();
		net::io_context ioc(num_threads);

		// автоматическое обновление времени
		if (args->tick_period_exist) {
			// strand, используемый для доступа к API
			auto api_strand = net::make_strand(ioc);
			// Настраиваем вызов метода Application::Tick
			auto ticker = std::make_shared<ticker::Ticker>(
				api_strand,
				static_cast<std::chrono::milliseconds>(std::stoi(args->tick_period)),
				[&game](std::chrono::milliseconds period_ms) {
					game.SpendTime(period_ms);
				});
			ticker->Start();
		}
		// Подписываемся на сигналы и при их получении завершаем работу сервера
		net::signal_set signals(ioc, SIGINT, SIGTERM);

		namespace sys = boost::system;
		signals.async_wait([&ioc](const sys::error_code& ec,
			[[maybe_unused]] int signal_number) {
				if (!ec) {
					std::cout << "Signal "sv << signal_number << " received"sv << std::endl;
					ioc.stop();
				}
			});

		// 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM

		// 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
		auto handler = std::make_shared<http_handler::RequestHandler>(
			game, connection_pool, "lol/kek", net::make_strand(ioc));

		// 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
		const auto address = net::ip::make_address("0.0.0.0");
		std::string adress_str = address.to_string();
		constexpr net::ip::port_type port = 8080;
		http_server::ServerHttp(
			ioc, { address, port },
			[handler](auto&& req, auto&& adresss_ip, auto&& send) {
				(*handler)(std::forward<decltype(req)>(req),
					std::forward<decltype(adresss_ip)>(adresss_ip),
					std::forward<decltype(send)>(send));
			});

		LogStartServer(port, adress_str);

		// Эта надпись сообщает тестам о том, что сервер запущен и готов
		// обрабатывать запросы
		// std::cout << "Server has started..."sv << std::endl;

		// 6. Запускаем обработку асинхронных операций
		RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

		// Когда сервер запускается без указания пути к файлу с сохранённым состоянием, он должен стартовать с чистого листа. 
		// При получении сигнала о завершении работы сервер не должен создавать никаких файлов.
		if (args->state_file_exist) {
			// Когда сервер запускается с указанием пути к отсутствующему файлу состояния, он должен стартовать с чистого листа. 
			// При получении сигнала о завершении работы сервер должен сохранить состояние в файл.

			// Когда сервер запускается с указанием пути к существующему файлу состояния, он должен должен восстановить это состояние. 
			// При получении сигнала о завершении работы сервер должен сохранить обновлённое состояние.
			game.SerilizeState();
		}
	}
	catch (const std::exception& ex) {
		LogExitServer(EXIT_FAILURE, ex.what());
		// std::cerr << ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	LogExitServer(EXIT_SUCCESS, std::nullopt);
	return EXIT_SUCCESS;
}
