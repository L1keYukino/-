cmake_minimum_required(VERSION 3.21)
project(voice_input_method VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Options ────────────────────────────────────────────
option(VIM_BUILD_TESTS "Build unit and integration tests" OFF)

# ─── The library uses only standard C++ in Phase 1. ─────
# Phase 2+ will add nlohmann/json, spdlog, Catch2 via FetchContent.

# ─── CMake modules ──────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ─── Library target ─────────────────────────────────────
add_subdirectory(src)

# ─── Tests (Phase 2+) ───────────────────────────────────
if(VIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
