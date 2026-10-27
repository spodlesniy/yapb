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

include(CTest)
if(BUILD_TESTING)
    add_executable(aipb_explorer_contracts_tests tests/contracts_tests.cpp)
    target_link_libraries(aipb_explorer_contracts_tests PRIVATE aipb::explorer_core)
    add_test(NAME aipb_explorer_contracts COMMAND aipb_explorer_contracts_tests)

    add_executable(aipb_explorer_environment_tests tests/environment_tests.cpp)
    target_link_libraries(aipb_explorer_environment_tests PRIVATE aipb::explorer_core)
    add_test(NAME aipb_explorer_environment COMMAND aipb_explorer_environment_tests)
endif()

set(aipb_explorer_targets aipb_explorer_core)
if(BUILD_TESTING)
    list(APPEND aipb_explorer_targets aipb_explorer_contracts_tests aipb_explorer_environment_tests)
endif()
foreach(target IN LISTS aipb_explorer_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
