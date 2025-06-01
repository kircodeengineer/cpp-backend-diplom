docker run --rm -p 80:8080 my_http_server

Запуск docker-compose

sudo docker-compose up -d

Остановка docker-compose

docker-compose down -v

Логи

docker-compose logs -f postgres

Проверьте создание базы данных

docker-compose exec postgres psql -U postgres -c "\l"


