add_executable(test_voice_input
    unit/test_config.cpp
    unit/test_state_machine.cpp
    unit/test_prompt_templates.cpp
    unit/test_context_manager.cpp
)

target_link_libraries(test_voice_input PRIVATE
    voice_input_method
    Catch2::Catch2WithMain
)

# Static linking to avoid DLL dependency issues with MinGW
if(MINGW)
    target_link_options(test_voice_input PRIVATE -static -static-libgcc -static-libstdc++)
endif()

target_include_directories(test_voice_input PRIVATE
    "${CMAKE_SOURCE_DIR}"
)

target_compile_definitions(test_voice_input PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

include("${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
set_project_warnings(test_voice_input)
target_compile_options(test_voice_input PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-sign-conversion>
)

include(Catch)
catch_discover_tests(test_voice_input)
