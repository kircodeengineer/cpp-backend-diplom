#pragma once
#include <boost/log/core.hpp>  // для logging::core
#include <boost/log/expressions.hpp>  // для выражения, задающего фильтр
#include <boost/log/trivial.hpp>  // для BOOST_LOG_TRIVIAL
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#define LOG(X) logger::LoggerSingleton::GetInstance().Log(X)

namespace logger {
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
using namespace std::literals;

struct Literals {
  Literals() = delete;
  constexpr static std::string_view TIMESTAMP = "timestamp"sv;
  constexpr static std::string_view DATA = "data"sv;
  constexpr static std::string_view MESSAGE = "message"sv;
  constexpr static std::string_view IP = "ip"sv;
  constexpr static std::string_view URI = "URI"sv;
  constexpr static std::string_view METHOD = "method"sv;
  constexpr static std::string_view PORT = "port"sv;
  constexpr static std::string_view ADDRESS = "address"sv;
  constexpr static std::string_view CODE = "code"sv;
  constexpr static std::string_view EXCEPTION = "exception"sv;
  constexpr static std::string_view CONTENT_TYPE = "content_type"sv;
  constexpr static std::string_view RESPONSE_TIME = "response_time"sv;
  constexpr static std::string_view WHERE = "where"sv;
  constexpr static std::string_view TEXT = "text"sv;
};

inline void MyFormatter(logging::record_view const& rec,
                        logging::formatting_ostream& strm) {
  // выводим само сообщение
  strm << rec[logging::expressions::smessage];
}

class LoggerSingleton {
 private:
  // конструктор синглтона приватный
  LoggerSingleton() { InitBoostLogFilter(); };

  void InitBoostLogFilter() {
    logging::add_common_attributes();
    logging::add_console_log(std::cout,  // в консоль clog
                             keywords::format = &MyFormatter,
                             keywords::auto_flush = true);
  }

 public:
  // Убираем конструктор копирования
  LoggerSingleton(const LoggerSingleton&) = delete;
  LoggerSingleton& operator=(const LoggerSingleton&) = delete;
  LoggerSingleton(LoggerSingleton&&) = delete;
  LoggerSingleton& operator=(LoggerSingleton&&) = delete;

  // Получение ссылки на единственный объект
  static LoggerSingleton& GetInstance() {
    static LoggerSingleton obj;
    return obj;
  }

  void Log(std::string_view message) { BOOST_LOG_TRIVIAL(info) << message; }
};
}  // namespace logger
