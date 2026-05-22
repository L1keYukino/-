# Collect all sources
file(GLOB_RECURSE VIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(voice_input_method STATIC ${VIM_SOURCES})

target_include_directories(voice_input_method
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}"
)

# Phase 1: no external dependencies (pure std C++)
# Phase 2+: target_link_libraries(voice_input_method PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog)

include("${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
set_project_warnings(voice_input_method)
