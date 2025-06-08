cmake_minimum_required(VERSION 3.10)
project(MyAwesomeProject LANGUAGES CXX)

# THIS CLASS IS A CMAKE FILE FOR A C++ PROJECT
# IT IS USED TO COMPILE THE PROJECT WITH CMAKE (WICH GENERATES A MAKEFILE)

# c++ 17 as asked in the project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# flag 03 is the best level of optimisation
# ggdb is to have more debug infos
# march_native is to optimise in regards of the processor
# W - flagd are for the compiler to warn us about potential issues
set(AGGRESSIVE_CXX_FLAGS "-O3 -ggdb -march=native -Wall -Wextra -Wpedantic -Werror")
set(AGGRESSIVE_CXX_FLAGS "${AGGRESSIVE_CXX_FLAGS} -Wshadow -Wconversion -Wold-style-cast -Wsign-promo")
set(AGGRESSIVE_CXX_FLAGS "${AGGRESSIVE_CXX_FLAGS} -Wnon-virtual-dtor -Wundef -Wformat=2 -Wformat-security")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AGGRESSIVE_CXX_FLAGS}")

#our files
add_executable(MyMatchingEngine main.cpp app_config.cpp argparse.cpp order.cpp orderbook.cpp)
target_include_directories(MyMatchingEngine PUBLIC "${PROJECT_SOURCE_DIR}/include")
