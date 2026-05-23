cmake_minimum_required(VERSION 3.21)
project(voice_input_method VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Options ────────────────────────────────────────────
option(VIM_BUILD_TESTS "Build unit and integration tests" ON)
option(VIM_USE_PORTAUDIO "Use PortAudio for real audio capture" ON)

# ─── CMake modules ──────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ─── FetchContent ───────────────────────────────────────
include(FetchContent)

# nlohmann/json (header-only JSON)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

# spdlog (header-only logging)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1)
FetchContent_MakeAvailable(spdlog)

# Catch2 (test framework)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.0)
FetchContent_MakeAvailable(Catch2)

# PortAudio (real audio capture)
if(VIM_USE_PORTAUDIO)
    FetchContent_Declare(portaudio
        GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
        GIT_TAG v19.7.0)
    FetchContent_MakeAvailable(portaudio)
endif()

# ─── Library target ─────────────────────────────────────
add_subdirectory(src)

# ─── Tests ──────────────────────────────────────────────
if(VIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
