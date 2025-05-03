cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(GAME_SERVER_STATIC_LIB game_server_static_lib)
add_library(${GAME_SERVER_STATIC_LIB} src/loot_generator.cpp )

add_executable(${PROJECT_NAME}
	src/collision_detector.cpp
	src/collision_detector.h
	src/geom.h
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/my_logger.h
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/postgres.h
	src/postgres.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/ticker.cpp
	src/ticker.h
	src/random_functions.cpp
	src/random_functions.h
)

set(GAME_SERVER_TESTS game_server_tests)
add_executable(${GAME_SERVER_TESTS}
	tests/loot_generator_tests.cpp
)

target_include_directories(${PROJECT_NAME} 
PRIVATE Threads::Threads 
CONAN_PKG::boost)

target_link_libraries(${PROJECT_NAME} 
${GAME_SERVER_STATIC_LIB} 
CONAN_PKG::boost
CONAN_PKG::libpqxx)

target_link_libraries(${GAME_SERVER_TESTS} 
PRIVATE 
${GAME_SERVER_STATIC_LIB}
CONAN_PKG::catch2 
CONAN_PKG::boost) 
