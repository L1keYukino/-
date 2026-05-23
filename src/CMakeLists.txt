file(GLOB_RECURSE VIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(voice_input_method STATIC ${VIM_SOURCES})

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

include("${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
set_project_warnings(voice_input_method)

# Turn off sign-conversion warnings (too noisy with external API types)
target_compile_options(voice_input_method PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-sign-conversion>
)
