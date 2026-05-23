file(GLOB_RECURSE VIM_LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
# Exclude main.cpp from the library
list(FILTER VIM_LIB_SOURCES EXCLUDE REGEX "main\\.cpp$")

add_library(voice_input_method STATIC ${VIM_LIB_SOURCES})

target_include_directories(voice_input_method
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(voice_input_method
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# PortAudio — conditionally linked
if(VIM_USE_PORTAUDIO AND TARGET PortAudio)
    target_link_libraries(voice_input_method PRIVATE PortAudio)
    target_compile_definitions(voice_input_method PRIVATE VIM_HAS_PORTAUDIO=1)
endif()

# sherpa-onnx — conditionally linked (MinGW links directly against DLL)
set(SHERPA_ONNX_DIR "${CMAKE_SOURCE_DIR}/models/sherpa-onnx")
if(EXISTS "${SHERPA_ONNX_DIR}/include/sherpa-onnx/c-api/c-api.h")
    target_include_directories(voice_input_method PRIVATE "${SHERPA_ONNX_DIR}/include")
    # MinGW can link directly against the DLL to auto-generate import lib
    target_link_libraries(voice_input_method PRIVATE
        "${SHERPA_ONNX_DIR}/lib/sherpa-onnx-c-api.dll"
    )
    target_compile_definitions(voice_input_method PRIVATE VIM_HAS_SHERPA_ONNX=1)
    message(STATUS "sherpa-onnx SDK found at ${SHERPA_ONNX_DIR}")
else()
    message(STATUS "sherpa-onnx SDK not found — ASR stub mode")
endif()

include("${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
set_project_warnings(voice_input_method)

# Turn off sign-conversion warnings
target_compile_options(voice_input_method PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-sign-conversion>
)

# ─── Executable target ──────────────────────────────────

add_executable(voice_input_method_app
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

target_link_libraries(voice_input_method_app PRIVATE
    voice_input_method
)

# Static linking to avoid DLL runtime dependencies
if(MINGW)
    target_link_options(voice_input_method_app PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(voice_input_method_app PRIVATE winhttp)
endif()

set_project_warnings(voice_input_method_app)
target_compile_options(voice_input_method_app PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-sign-conversion>
)
