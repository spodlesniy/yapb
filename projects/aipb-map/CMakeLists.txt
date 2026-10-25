cmake_minimum_required(VERSION 3.21)

project(aipb_map VERSION 0.1.0 LANGUAGES CXX)

add_library(aipb_map_core STATIC
    src/core/version.cpp
    src/core/model.cpp
)
add_library(aipb::map_core ALIAS aipb_map_core)
target_compile_features(aipb_map_core PUBLIC cxx_std_20)
target_include_directories(aipb_map_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_definitions(aipb_map_core PRIVATE AIPB_MAP_VERSION="${PROJECT_VERSION}")

add_library(aipb_map_io STATIC
    src/io/json.cpp
    src/io/sha256.cpp
    src/io/artifact.cpp
)
add_library(aipb::map_io ALIAS aipb_map_io)
target_link_libraries(aipb_map_io PUBLIC aipb::map_core)

add_executable(aipb-map src/cli/main.cpp)
target_link_libraries(aipb-map PRIVATE aipb::map_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(aipb_map_tests tests/version_tests.cpp)
    target_link_libraries(aipb_map_tests PRIVATE aipb::map_core)
    add_executable(aipb_map_model_tests tests/model_tests.cpp)
    target_link_libraries(aipb_map_model_tests PRIVATE aipb::map_io)
    target_include_directories(aipb_map_model_tests PRIVATE src/io)
    add_test(NAME aipb_map_models_and_artifacts COMMAND aipb_map_model_tests)
    add_test(NAME aipb_map_core_version COMMAND aipb_map_tests)
    add_test(NAME aipb_map_cli_version COMMAND aipb-map --version)
    set_tests_properties(aipb_map_cli_version PROPERTIES
        PASS_REGULAR_EXPRESSION "aipb-map ${PROJECT_VERSION}"
    )
endif()

set(aipb_map_targets aipb_map_core aipb_map_io aipb-map)
if(BUILD_TESTING)
    list(APPEND aipb_map_targets aipb_map_tests aipb_map_model_tests)
endif()
foreach(target IN LISTS aipb_map_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
