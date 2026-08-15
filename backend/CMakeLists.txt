cmake_minimum_required(VERSION 3.20)
project(academia_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Compiler warnings ---
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- FetchContent for dependencies ---
include(FetchContent)

# Crow HTTP Framework (header-only)
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
    GIT_SHALLOW    TRUE
)

# nlohmann/json (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# Prevent GoogleTest from overriding compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(crow nlohmann_json googletest)

# --- Find SQLite3 ---
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    # Fetch SQLite amalgamation if not found
    FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # Build sqlite3 as a static library from amalgamation
    FetchContent_GetProperties(sqlite3)
    if(NOT sqlite3_POPULATED)
        FetchContent_Populate(sqlite3)
        add_library(sqlite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
        target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
        if(UNIX)
            target_link_libraries(sqlite3_lib PUBLIC pthread dl)
        endif()
    endif()
    set(SQLITE3_LIBRARIES sqlite3_lib)
    set(SQLITE3_INCLUDE_DIRS ${sqlite3_SOURCE_DIR})
else()
    set(SQLITE3_LIBRARIES SQLite::SQLite3)
    set(SQLITE3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIRS})
endif()

# --- Find threading library ---
find_package(Threads REQUIRED)

# --- Main server library (shared between server and tests) ---
add_library(academia_core STATIC
    src/database/Database.cpp
)
target_include_directories(academia_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLITE3_INCLUDE_DIRS}
)
target_link_libraries(academia_core PUBLIC
    ${SQLITE3_LIBRARIES}
    nlohmann_json::nlohmann_json
    Crow::Crow
    Threads::Threads
)

# --- Main server executable ---
add_executable(academia_server src/main.cpp)
target_link_libraries(academia_server PRIVATE academia_core)

# --- Tests ---
enable_testing()

add_executable(academia_tests
    tests/test_auth.cpp
    tests/test_courses.cpp
    tests/test_registration.cpp
    tests/test_concurrency.cpp
)
target_link_libraries(academia_tests PRIVATE
    academia_core
    GTest::gtest_main
)
target_include_directories(academia_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(GoogleTest)
gtest_discover_tests(academia_tests)
