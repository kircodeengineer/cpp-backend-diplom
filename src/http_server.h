#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>  // время
#include <boost/json.hpp>

#include "my_logger.h"

// Ядро асинхронного HTTP-сервера будет рсаполагаться в пространстве имён
// http-server
namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace sys = boost::system;
namespace http = beast::http;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase {
 protected:
  using HttpRequest = http::request<http::string_body>;

 public:
  // Запрещаем копирование объектов класса и его наследников
  SessionBase() = delete;

  // Запрещаем перемещение объектов класса и его наследников
  SessionBase& operator=(const SessionBase&) = delete;

  /// @brief Для асинхронного запуска сессии
  void Run();

 protected:
  explicit SessionBase(tcp::socket&& socket);
  std::string ip_;

  // Класс SessionBase не предназначен для полиморфного удаления. Поэтому его
  // деструктор объявлен защищённым и невиртуальным, чтобы уменьшить накладные
  // расходы.
  ~SessionBase() = default;

  template <typename Body, typename Fields>
  void Write(http::response<Body, Fields>&& response) {
    // Чтобы продлить время жизни ответа до окончания записи,
    // надо переместить содержимое ответа в область кучи и сохранить умный
    // указатель на этот объект в списке захвата лямбда-функции.

    // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
    auto self_response =
        std::make_shared<http::response<Body, Fields>>(std::move(response));

    auto self = GetSharedThis();

    http::async_write(
        stream_, *self_response,
        [self_response, self](beast::error_code ec, std::size_t bytes_written) {
          self->OnWrite(self_response->need_eof(), ec, bytes_written);
        });
  }

 private:
  /// @brief Асинхронное чтение запроса. Может быть вызван несколько раз.
  void Read();

  void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);

  void OnWrite(bool close, beast::error_code ec,
               [[maybe_unused]] std::size_t bytes_written);

  void Close();

  virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
  virtual void HandleRequest(HttpRequest&& request) = 0;

 private:
  // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  HttpRequest request_;
};

/// @brief Этот класс будет отвечать за сеанс асинхронного обмена данными с
/// клиентом. Экземпляры этого класса будут размещаться в куче, чтобы не
/// разрушиться раньше времени. Для управления временем жизни экземпляров
/// Session будет использован приём с захватом указателя shared_ptr на текущий
/// объект.
/// @tparam RequestHandler
template <typename RequestHandler>
class Session : public SessionBase,
                public std::enable_shared_from_this<Session<RequestHandler>> {
  // Внутри Session управление по очереди получают методы : Read → OnRead →
  // HandleRequest → Write → OnWrite, а затем снова Read.Есть ли здесь риск
  // бесконечной рекурсии ? Здесь нет рекурсии, так как методы OnRead и OnWrite
  // вызываются асинхронно — их вызывает не сама программа, а операционная
  // система и Boost.Asio.
 public:
  template <typename Handler>
  Session(tcp::socket&& socket, Handler&& request_handler)
      : SessionBase(std::move(socket)),
        request_handler_(std::forward<Handler>(request_handler)){};

 private:
  void HandleRequest(HttpRequest&& request) override {
    // Захватываем умный указатель на текущий объект Session в лямбде,
    // чтобы продлить время жизни сессии до вызова лямбды.
    // Используется generic-лямбда функция, способная принять response
    // произвольного типа
    request_handler_(std::move(request), ip_,
                     [self = this->shared_from_this()](auto&& response) {
                       self->Write(std::move(response));
                     });
  }

  std::shared_ptr<SessionBase> GetSharedThis() override {
    return this->shared_from_this();
  }

 private:
  RequestHandler request_handler_;
};

// Зачем нужно задавать способ обработки запросов?
// Так код сервера будет соответствовать Принципу открытости-закрытости
// (Open-Close Principle). Это один из основных принципов
// объектно-ориентированного программирования — код должен быть открыт для
// расширения, но закрыт для внесения изменений.

/// @brief Шаблонный класс Слушатель асинхронно принимает входящие
/// TCP-соединения. Для этого он использует содержащийся в нём акцептор.
/// @tparam RequestHandler тип функции-обработчика запросов. Это позволяет, не
/// изменяя класс Listener, задавать способ обработки запросов.
template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
 private:
  // Ссылка на io_context, управляющий асинхронными операциями.
  // Контекст требуется для конструирования сокета, поэтому нужно сохранить
  // ссылку на этот контекст.
  net::io_context& ioc_;

  // приём соединений клиентов
  tcp::acceptor acceptor_;

  // обработчик запросов
  RequestHandler request_handler_;

 public:
  template <typename Handler>
  Listener(net::io_context& ioc, const tcp::endpoint& endpoint,
           Handler&& request_handler)
      : ioc_(ioc),
        // Чтобы гарантировать последовательный вызов асинхронных операций
        // класса acceptor, при его конструировании нужно передать обёрнутый в
        // strand контекст.
        acceptor_(net::make_strand(ioc)),
        request_handler_(std::forward<Handler>(request_handler)) {
    // открываем acceptor, используя протокол (IPv4 или IPv6), указанный в
    // endpoint
    acceptor_.open(endpoint.protocol());

    // После закрытия TCP-соединения сокет некоторое время может считаться
    // занятым, чтобы компьютеры могли обменяться завершающими пакетами данных.
    // Однако это может помешать повторно открыть сокет в полузакрытом
    // состоянии. Флаг reuse_adress разрешает открвть сокет, когда он
    // "наполовину закрыт"
    acceptor_.set_option(net::socket_base::reuse_address(true));

    // Привязываем acceptor к адресу и порту endpoint
    acceptor_.bind(endpoint);

    // Переводим acceptor в состояние, в котором он способен принимать новые
    // соединения Благодаря этому новые подключения будут помещаться в очередь
    // ожидающих соединений
    acceptor_.listen(net::socket_base::max_listen_connections);
  }

  void Run() { DoAccept(); }

 private:
  void DoAccept() {
    // Для асинхронного ожидания подключений.
    // Этот метод :
    // - асинхронно дождётся подключения клиента;
    // - создаст сокет, передав ему executor;
    // - вызовет функцию - обработчик и передаст в неё код ошибки и созданный
    // сокет.
    acceptor_.async_accept(
        // Передаем последовательный исполнитель, в котором будут вызываться
        // обработчики асинхронных операций сокета
        net::make_strand(ioc_),
        // Чтобы в качестве функции обработчика передать метод текущего объекта,
        // воспользуемся вспомогательной функцией beast::bind_front_handler. Она
        // привязывает к своему первому аргументу (функции или методу) остальные
        // аргументы и возвращает новый обработчик. Этот обработчик вызовет
        // обёрнутую функцию, передав ей привязанные аргументы, а следом за ними
        // — свои собственные.

        // С помощью bind_front_handler создаем обработчик, привязанный к методу
        // OnAccept текущего объекта. Так как Listener - шаблонный класс, нужно
        // подсказать компилятору что shared_from_this - метод класса, а не
        // свободная функция. Для этого вызываем его, используя this. Этот вызов
        // brind_from_handler аналогичен namespace ph = std::placeholders;
        // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1,
        // ph::_2)
        beast::bind_front_handler(&Listener::OnAccept,
                                  this->shared_from_this()));
  }

  void OnAccept(std::error_code ec, tcp::socket socket) {
    using namespace std::literals;

    if (ec) {
      return ReportError(ec, "accept"sv);
    }

    // Асинхронно обрабатываем сессии
    AsyncRunSession(std::move(socket));

    // Принимаем новое сообщение
    DoAccept();
  }

  void AsyncRunSession(tcp::socket&& socket) {
    // В Listener::AsyncRunSession сконструируйте сессию, используя
    // std::make_shared и вызовите Run.
    std::make_shared<Session<RequestHandler>>(std::move(socket),
                                              request_handler_)
        ->Run();
  }
};

/// @brief Вспомогательная функция для запуска сервера
/// @tparam RequestHandler
/// @param ioc
/// @param endpoint
/// @param request_handler
template <typename RequestHandler>
void ServerHttp(net::io_context& ioc, const tcp::endpoint& endpoint,
                RequestHandler&& handler) {
  // При помощи decay_t исключим ссылки из типа RequestHandler,
  // чтобы Listener хранил RequestHandler по значению
  using MyListener = Listener<std::decay_t<RequestHandler>>;

  std::make_shared<MyListener>(ioc, endpoint,
                               std::forward<RequestHandler>(handler))
      ->Run();
}
};  // namespace http_server
