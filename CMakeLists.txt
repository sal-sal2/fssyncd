cmake_minimum_required(VERSION 3.20)
project(lan-drop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  
                                      
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)


include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2   
)
FetchContent_MakeAvailable(asio)

add_library(asio_standalone INTERFACE)

target_include_directories(asio_standalone INTERFACE
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(asio_standalone INTERFACE
    ASIO_STANDALONE   
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium>=1.0.18)



# Main
add_executable(lan-drop
    src/main.cpp
)

target_link_libraries(lan-drop
    PRIVATE
        asio_standalone
        PkgConfig::SODIUM
        Threads::Threads
)

# Tests
add_subdirectory(tests)