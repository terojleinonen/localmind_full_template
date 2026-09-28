cmake_minimum_required(VERSION 3.16)
project(LocalMind VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LOCALMIND_BUILD_TESTS "Build unit and API tests" ON)
option(LOCALMIND_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
option(LOCALMIND_WERROR "Treat compiler warnings as errors" OFF)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Vendored single-header libraries (marked SYSTEM so their warnings stay quiet).
add_library(localmind_third_party INTERFACE)
target_include_directories(localmind_third_party SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party)

add_library(localmind_core STATIC
    src/Config.cpp
    src/Embedder.cpp
    src/LlmClient.cpp
    src/Log.cpp
    src/QueryEngine.cpp
    src/Server.cpp
    src/TextProcessor.cpp
    src/VectorStore.cpp
)
target_include_directories(localmind_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(localmind_core PUBLIC
    localmind_third_party SQLite::SQLite3 Threads::Threads)
target_compile_definitions(localmind_core PUBLIC
    LOCALMIND_VERSION="${PROJECT_VERSION}")
target_compile_options(localmind_core PRIVATE -Wall -Wextra -Wpedantic)
if(LOCALMIND_WERROR)
    target_compile_options(localmind_core PRIVATE -Werror)
endif()

if(LOCALMIND_SANITIZE)
    target_compile_options(localmind_core PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(localmind_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(localmind_server src/main.cpp)
target_link_libraries(localmind_server PRIVATE localmind_core)
target_compile_options(localmind_server PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS localmind_server RUNTIME DESTINATION bin)
install(DIRECTORY web/ DESTINATION share/localmind/web)

if(LOCALMIND_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
