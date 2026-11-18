cmake_minimum_required(VERSION 3.24)
project(agar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Исходники в UTF-8. Без этого MSVC читает файл в системной кодировке,
# а SFML 3 ожидает UTF-8 в sf::Text — русские строки становятся крокозябрами.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

include(FetchContent)

# Официальный Boost. Репозиторий boostorg/asio сам по себе не собирается:
# ему нужны заголовки system, config, assert и остальных модулей Boost.
# Архив релиза — снимок тех же репозиториев boostorg/*, не отдельный форк.
# BOOST_INCLUDE_LIBRARIES оставляет в сборке только Asio и его зависимости.
set(BOOST_INCLUDE_LIBRARIES asio)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(Boost)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.1
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(Catch2)

add_library(agar_common
    common/world.cpp)
target_include_directories(agar_common PUBLIC common)
target_link_libraries(agar_common PUBLIC Boost::asio)

add_executable(agar_server
    server/main.cpp
    server/tcp_session.cpp
    server/udp_server.cpp)
target_include_directories(agar_server PRIVATE server)
target_link_libraries(agar_server PRIVATE agar_common)

add_executable(agar_client
    client/main.cpp
    client/client.cpp)
target_include_directories(agar_client PRIVATE client)
target_link_libraries(agar_client PRIVATE agar_common SFML::Graphics)

add_executable(agar_tests tests/world_test.cpp)
target_link_libraries(agar_tests PRIVATE agar_common Catch2::Catch2WithMain)
