cmake_minimum_required(VERSION 3.20)
project(lan-drop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE
  ASIO_STANDALONE
  ASIO_NO_DEPRECATED
)
target_link_libraries(asio INTERFACE Threads::Threads)

find_package(Sodium REQUIRED)

add_executable(lan-drop src/main.cpp)

target_include_directories(lan-drop PRIVATE include)
target_link_libraries(lan-drop PRIVATE asio sodium)

target_compile_options(lan-drop PRIVATE
  -Wall -Wextra -Wpedantic
)
