# Установка docker compose v2 для всех пользователей
## Пошаговая инструкция

1. Создайте системный каталог для Docker Compose:
sudo mkdir -p /usr/local/lib/docker/cli-plugins/

2. Скачайте последнюю версию Docker Compose:

sudo curl -SL https://github.com/docker/compose/releases/download/v2.17.2/docker-compose-linux-x86_64 -o /usr/local/lib/docker/cli-plugins/docker-compose

3. Настройте права доступа:

sudo chmod +x /usr/local/lib/docker/cli-plugins/docker-compose

4. Создайте символическую ссылку для доступности:

sudo ln -s /usr/local/lib/docker/cli-plugins/docker-compose /usr/local/bin/docker-compose

5. Проверьте установку:

docker compose version

## Альтернативный способ установки через snap

1. Установите snapd (если еще не установлен):

sudo apt update

sudo apt install snapd

2. Установите Docker Compose через snap:

sudo snap install docker-compose

## Проверка установки

После установки проверьте доступность для всех пользователей:

which docker-compose

docker compose version

# Использование

Для сборки проекта:

docker compose build

Для запуска контейнеров:

docker compose up

## Запуск микросервисисов

Запуск docker-compose

sudo docker compose up -d

Остановка docker-compose

docker compose down -v

Логи

docker compose logs -f postgres

Проверьте создание базы данных

docker compose exec postgres psql -U postgres -c "\l"
