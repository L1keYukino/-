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

# PortAudio — compiled directly from local clone in models/portaudio
set(PORTAUDIO_DIR "${CMAKE_SOURCE_DIR}/models/portaudio")
if(VIM_USE_PORTAUDIO AND EXISTS "${PORTAUDIO_DIR}/include/portaudio.h")
    file(GLOB PA_COMMON "${PORTAUDIO_DIR}/src/common/*.c")
    file(GLOB PA_WIN   "${PORTAUDIO_DIR}/src/os/win/*.c")

    target_include_directories(voice_input_method PRIVATE
        "${PORTAUDIO_DIR}/include"
        "${PORTAUDIO_DIR}/src/common"
        "${PORTAUDIO_DIR}/src/os/win"
        "${PORTAUDIO_DIR}/src/hostapi/wasapi"
        "${PORTAUDIO_DIR}/src/hostapi/wdmks"
        "${PORTAUDIO_DIR}/src/hostapi/dsound"
        "${PORTAUDIO_DIR}/src/hostapi/wmme"
    )

    target_sources(voice_input_method PRIVATE
        ${PA_COMMON}
        ${PA_WIN}
        "${PORTAUDIO_DIR}/src/hostapi/wasapi/pa_win_wasapi.c"
        "${PORTAUDIO_DIR}/src/hostapi/wdmks/pa_win_wdmks.c"
        "${PORTAUDIO_DIR}/src/hostapi/dsound/pa_win_ds.c"
        "${PORTAUDIO_DIR}/src/hostapi/dsound/pa_win_ds_dynlink.c"
        "${PORTAUDIO_DIR}/src/hostapi/wmme/pa_win_wmme.c"
    )

    target_compile_definitions(voice_input_method PRIVATE
        VIM_HAS_PORTAUDIO=1
        PA_USE_WASAPI=1
        PA_USE_WDMKS=1
        PA_USE_DS=1
        PA_USE_WMME=1
    )

    # PortAudio needs these Windows libs
    if(WIN32)
        target_link_libraries(voice_input_method PRIVATE
            winmm ole32 uuid ksuser setupapi)
    endif()

    message(STATUS "PortAudio: compiled directly from models/portaudio")
else()
    message(STATUS "PortAudio: not found — audio stub mode")
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

add_executable(voice_input_method_app WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

# ─── Shared library (DLL) for Python UI ─────────────────
add_library(vim_engine SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/engine_api.cpp"
)

target_link_libraries(vim_engine PRIVATE voice_input_method)

if(MINGW)
    target_link_options(vim_engine PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(vim_engine PRIVATE winhttp winmm ole32 uuid ksuser setupapi)
endif()

target_link_libraries(voice_input_method_app PRIVATE
    voice_input_method
)

# Static linking to avoid DLL runtime dependencies
if(MINGW)
    target_link_options(voice_input_method_app PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(voice_input_method_app PRIVATE winhttp gdiplus)
endif()

set_project_warnings(voice_input_method_app)
target_compile_options(voice_input_method_app PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-sign-conversion>
)

# llama.cpp — conditionally linked (local clone in models/llama.cpp)
if(TARGET llama)
    target_link_libraries(voice_input_method PRIVATE llama)
    target_include_directories(voice_input_method PRIVATE
        "${CMAKE_SOURCE_DIR}/models/llama.cpp/include"
        "${CMAKE_SOURCE_DIR}/models/llama.cpp"
        "${CMAKE_BINARY_DIR}/_deps/llama-build"
    )
    message(STATUS "llama.cpp: linked from models/llama.cpp")
else()
    message(STATUS "llama.cpp: not available — LLM stub mode")
endif()
