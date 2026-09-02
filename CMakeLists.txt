cmake_minimum_required(VERSION 3.20)
project(fssyncd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  
                                      
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium>=1.0.18)

enable_testing()


include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2   
)
FetchContent_MakeAvailable(asio)

add_library(asio_standalone INTERFACE)

target_include_directories(asio_standalone INTERFACE ${asio_SOURCE_DIR}/asio/include)

target_compile_definitions(asio_standalone INTERFACE ASIO_STANDALONE)


add_library(fssyncd_core
    src/file_watcher.cpp
    src/message.cpp
    src/encryptor.cpp
    src/thread_pool.cpp
    src/hasher.cpp
    src/networking.cpp
    src/peer_connector.cpp
    src/sync_engine.cpp
    src/sync_daemon.cpp
    src/config.cpp
)

target_include_directories(fssyncd_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(fssyncd_core PUBLIC
    Threads::Threads
    PkgConfig::SODIUM
    asio_standalone
)

# Main
add_executable(fssyncd src/fssyncd.cpp)

target_link_libraries(fssyncd PRIVATE fssyncd_core)
