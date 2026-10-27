cmake_minimum_required(VERSION 3.21)
project(aipb_explorer VERSION 0.1.0 LANGUAGES CXX)

add_library(aipb_explorer_core STATIC
    src/core/validation.cpp
    src/core/episode.cpp
)
add_library(aipb::explorer_core ALIAS aipb_explorer_core)
target_compile_features(aipb_explorer_core PUBLIC cxx_std_20)
target_include_directories(aipb_explorer_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(aipb_explorer_goldsrc STATIC src/engine/goldsrc_environment.cpp)
add_library(aipb::explorer_goldsrc ALIAS aipb_explorer_goldsrc)
target_link_libraries(aipb_explorer_goldsrc PUBLIC aipb::explorer_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(aipb_explorer_contracts_tests tests/contracts_tests.cpp)
    target_link_libraries(aipb_explorer_contracts_tests PRIVATE aipb::explorer_core)
    add_test(NAME aipb_explorer_contracts COMMAND aipb_explorer_contracts_tests)

    add_executable(aipb_explorer_environment_tests tests/environment_tests.cpp)
    target_link_libraries(aipb_explorer_environment_tests PRIVATE aipb::explorer_core)
    add_test(NAME aipb_explorer_environment COMMAND aipb_explorer_environment_tests)
    add_executable(aipb_explorer_goldsrc_tests tests/goldsrc_environment_tests.cpp)
    target_link_libraries(aipb_explorer_goldsrc_tests PRIVATE aipb::explorer_goldsrc)
    add_test(NAME aipb_explorer_goldsrc_environment COMMAND aipb_explorer_goldsrc_tests)

    add_executable(aipb_explorer_bridge_trace_tests tests/engine_bridge_trace_tests.cpp)
    add_test(NAME aipb_explorer_bridge_trace COMMAND aipb_explorer_bridge_trace_tests)
    target_compile_features(aipb_explorer_bridge_trace_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_bridge_trace_tests PRIVATE include)

    add_executable(aipb_explorer_bridge_math_tests tests/engine_bridge_math_tests.cpp)
    add_test(NAME aipb_explorer_bridge_math COMMAND aipb_explorer_bridge_math_tests)
    target_compile_features(aipb_explorer_bridge_math_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_bridge_math_tests PRIVATE include)
endif()

set(aipb_explorer_targets aipb_explorer_core aipb_explorer_goldsrc)
if(BUILD_TESTING)
    list(APPEND aipb_explorer_targets aipb_explorer_contracts_tests aipb_explorer_environment_tests aipb_explorer_goldsrc_tests aipb_explorer_bridge_math_tests aipb_explorer_bridge_trace_tests)
endif()
foreach(target IN LISTS aipb_explorer_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
