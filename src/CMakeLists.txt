cmake_minimum_required(VERSION 3.10)
project(MyAwesomeProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AGGRESSIVE_CXX_FLAGS "-O3 -ggdb -march=native -Wall -Wextra -Wpedantic -Werror")
set(AGGRESSIVE_CXX_FLAGS "${AGGRESSIVE_CXX_FLAGS} -Wshadow -Wconversion -Wold-style-cast -Wsign-promo")
set(AGGRESSIVE_CXX_FLAGS "${AGGRESSIVE_CXX_FLAGS} -Wnon-virtual-dtor -Wundef -Wformat=2 -Wformat-security")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AGGRESSIVE_CXX_FLAGS}")

add_executable(MyMatchingEngine main.cpp app_config.cpp argparse.cpp order.cpp orderbook.cpp)
target_include_directories(MyMatchingEngine PUBLIC "${PROJECT_SOURCE_DIR}/include")
