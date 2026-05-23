cmake_minimum_required(VERSION 3.21)
project(voice_input_method VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# PortAudio (real audio capture) — pre-cloned in models/portaudio, compiled directly

# llama.cpp (local LLM inference) — pre-cloned in models/llama.cpp
option(VIM_USE_LLAMACPP "Build llama.cpp LLM backend" ON)
if(VIM_USE_LLAMACPP AND EXISTS "${CMAKE_SOURCE_DIR}/models/llama.cpp/CMakeLists.txt")
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_SOURCE_DIR}/models/llama.cpp"
                     "${CMAKE_BINARY_DIR}/_deps/llama-build"
                     EXCLUDE_FROM_ALL)
    message(STATUS "llama.cpp: using local clone in models/")
elseif(VIM_USE_LLAMACPP)
    message(STATUS "llama.cpp: not found — run: git clone --depth 1 --branch b4927 https://github.com/ggml-org/llama.cpp.git models/llama.cpp")
endif()

# ─── Library target ─────────────────────────────────────
add_subdirectory(src)

# ─── Tests ──────────────────────────────────────────────
if(VIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
