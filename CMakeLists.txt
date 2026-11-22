cmake_minimum_required(VERSION 3.16)
project(PaddleTeamTracker LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SQLite, compiled from the single source file kept in third_party/.
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=0            # the program uses one thread
    SQLITE_OMIT_LOAD_EXTENSION     # no plug-ins, so no extra system libraries are needed
    SQLITE_DQS=0)                  # treat "double quotes" strictly, as the SQL standard does

# Core: data, storage and calculations. No input or output.
add_library(ptt_core STATIC
    src/core/database.cpp
    src/core/format.cpp
    src/core/team_store.cpp
    src/core/csv.cpp
    src/core/sample_data.cpp)
target_link_libraries(ptt_core PUBLIC sqlite3)

# The console menus, as a library so the tests can drive them too.
add_library(ptt_app STATIC
    src/app/console.cpp
    src/app/app.cpp)
target_link_libraries(ptt_app PUBLIC ptt_core)

add_executable(paddle-team-tracker src/app/main.cpp)
target_link_libraries(paddle-team-tracker PRIVATE ptt_app)

# Tests: build them, then run "ctest" (or run ptt_tests directly).
enable_testing()
add_executable(ptt_tests tests/tests.cpp)
target_link_libraries(ptt_tests PRIVATE ptt_app)
add_test(NAME ptt_tests COMMAND ptt_tests)

# Strict warnings for this project's own code (not for SQLite).
foreach(target ptt_core ptt_app paddle-team-tracker ptt_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endforeach()
