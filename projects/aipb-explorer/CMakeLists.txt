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

    add_executable(aipb_explorer_named_trace_io_tests tests/engine_bridge_trace_io_tests.cpp)
    add_test(NAME aipb_explorer_named_trace_io COMMAND aipb_explorer_named_trace_io_tests)
    target_compile_features(aipb_explorer_named_trace_io_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_named_trace_io_tests PRIVATE include)

    add_executable(aipb_explorer_queue_tests tests/engine_bridge_queue_tests.cpp)
    add_test(NAME aipb_explorer_queue COMMAND aipb_explorer_queue_tests)
    target_compile_features(aipb_explorer_queue_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_queue_tests PRIVATE include)

    add_executable(aipb_explorer_bridge_trace_tests tests/engine_bridge_trace_tests.cpp)
    add_test(NAME aipb_explorer_bridge_trace COMMAND aipb_explorer_bridge_trace_tests)
    target_compile_features(aipb_explorer_bridge_trace_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_bridge_trace_tests PRIVATE include)

    add_executable(aipb_explorer_trace_report_tests tests/engine_bridge_trace_report_tests.cpp)
    add_test(NAME aipb_explorer_trace_report COMMAND aipb_explorer_trace_report_tests)
    target_compile_features(aipb_explorer_trace_report_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_trace_report_tests PRIVATE include)

    add_executable(aipb_explorer_trace_classification_tests tests/engine_bridge_trace_classification_tests.cpp)
    add_test(NAME aipb_explorer_trace_classification COMMAND aipb_explorer_trace_classification_tests)
    target_compile_features(aipb_explorer_trace_classification_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_trace_classification_tests PRIVATE include)

    add_executable(aipb_explorer_trace_observations_tests tests/engine_bridge_trace_observations_tests.cpp)
    add_test(NAME aipb_explorer_trace_observations COMMAND aipb_explorer_trace_observations_tests)
    target_compile_features(aipb_explorer_trace_observations_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_trace_observations_tests PRIVATE include)

    add_executable(aipb_explorer_observation_collection_tests tests/engine_bridge_observation_collection_tests.cpp)
    add_test(NAME aipb_explorer_observation_collection COMMAND aipb_explorer_observation_collection_tests)
    target_compile_features(aipb_explorer_observation_collection_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_observation_collection_tests PRIVATE include)

    add_executable(aipb_explorer_spatial_index_tests tests/engine_bridge_spatial_index_tests.cpp)
    add_test(NAME aipb_explorer_spatial_index COMMAND aipb_explorer_spatial_index_tests)
    target_compile_features(aipb_explorer_spatial_index_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_spatial_index_tests PRIVATE include)

    add_executable(aipb_explorer_area_statistics_tests tests/engine_bridge_area_statistics_tests.cpp)
    add_test(NAME aipb_explorer_area_statistics COMMAND aipb_explorer_area_statistics_tests)
    target_compile_features(aipb_explorer_area_statistics_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_area_statistics_tests PRIVATE include)

    add_executable(aipb_explorer_direction_evidence_tests tests/engine_bridge_direction_evidence_tests.cpp)
    add_test(NAME aipb_explorer_direction_evidence COMMAND aipb_explorer_direction_evidence_tests)
    target_compile_features(aipb_explorer_direction_evidence_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_direction_evidence_tests PRIVATE include)

    add_executable(aipb_explorer_mixed_evidence_audit_tests tests/engine_bridge_mixed_evidence_audit_tests.cpp)
    add_test(NAME aipb_explorer_mixed_evidence_audit COMMAND aipb_explorer_mixed_evidence_audit_tests)
    target_compile_features(aipb_explorer_mixed_evidence_audit_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_mixed_evidence_audit_tests PRIVATE include)

    add_executable(aipb_explorer_direction_assessment_tests tests/engine_bridge_direction_assessment_tests.cpp)
    add_test(NAME aipb_explorer_direction_assessment COMMAND aipb_explorer_direction_assessment_tests)
    target_compile_features(aipb_explorer_direction_assessment_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_direction_assessment_tests PRIVATE include)

    add_executable(aipb_explorer_trial_corroboration_tests tests/engine_bridge_trial_corroboration_tests.cpp)
    add_test(NAME aipb_explorer_trial_corroboration COMMAND aipb_explorer_trial_corroboration_tests)
    target_compile_features(aipb_explorer_trial_corroboration_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_trial_corroboration_tests PRIVATE include)

    add_executable(aipb_explorer_source_consensus_tests tests/engine_bridge_source_consensus_tests.cpp)
    add_test(NAME aipb_explorer_source_consensus COMMAND aipb_explorer_source_consensus_tests)
    target_compile_features(aipb_explorer_source_consensus_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_source_consensus_tests PRIVATE include)

    add_executable(aipb_explorer_spatial_localization_tests tests/engine_bridge_spatial_localization_tests.cpp)
    add_test(NAME aipb_explorer_spatial_localization COMMAND aipb_explorer_spatial_localization_tests)
    target_compile_features(aipb_explorer_spatial_localization_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_spatial_localization_tests PRIVATE include)

    add_executable(aipb_explorer_spatial_regions_tests tests/engine_bridge_spatial_regions_tests.cpp)
    add_test(NAME aipb_explorer_spatial_regions COMMAND aipb_explorer_spatial_regions_tests)
    target_compile_features(aipb_explorer_spatial_regions_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_spatial_regions_tests PRIVATE include)

    add_executable(aipb_explorer_cross_cell_regions_tests tests/engine_bridge_cross_cell_regions_tests.cpp)
    add_test(NAME aipb_explorer_cross_cell_regions COMMAND aipb_explorer_cross_cell_regions_tests)
    target_compile_features(aipb_explorer_cross_cell_regions_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_cross_cell_regions_tests PRIVATE include)

    add_executable(aipb_explorer_boundary_crossings_tests tests/engine_bridge_boundary_crossings_tests.cpp)
    add_test(NAME aipb_explorer_boundary_crossings COMMAND aipb_explorer_boundary_crossings_tests)
    target_compile_features(aipb_explorer_boundary_crossings_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_boundary_crossings_tests PRIVATE include)

    add_executable(aipb_explorer_crossing_discovery_tests tests/engine_bridge_crossing_discovery_tests.cpp)
    add_test(NAME aipb_explorer_crossing_discovery COMMAND aipb_explorer_crossing_discovery_tests)
    target_compile_features(aipb_explorer_crossing_discovery_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_crossing_discovery_tests PRIVATE include)

    add_executable(aipb_explorer_crossing_directions_tests tests/engine_bridge_crossing_directions_tests.cpp)
    add_test(NAME aipb_explorer_crossing_directions COMMAND aipb_explorer_crossing_directions_tests)
    target_compile_features(aipb_explorer_crossing_directions_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_crossing_directions_tests PRIVATE include)

    add_executable(aipb_explorer_directed_crossing_consensus_tests tests/engine_bridge_directed_crossing_consensus_tests.cpp)
    add_test(NAME aipb_explorer_directed_crossing_consensus COMMAND aipb_explorer_directed_crossing_consensus_tests)
    target_compile_features(aipb_explorer_directed_crossing_consensus_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_directed_crossing_consensus_tests PRIVATE include)

    add_executable(aipb_explorer_crossing_sectors_tests tests/engine_bridge_crossing_sectors_tests.cpp)
    add_test(NAME aipb_explorer_crossing_sectors COMMAND aipb_explorer_crossing_sectors_tests)
    target_compile_features(aipb_explorer_crossing_sectors_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_crossing_sectors_tests PRIVATE include)

    add_executable(aipb_explorer_fine_crossings_tests tests/engine_bridge_fine_crossings_tests.cpp)
    add_test(NAME aipb_explorer_fine_crossings COMMAND aipb_explorer_fine_crossings_tests)
    target_compile_features(aipb_explorer_fine_crossings_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_crossings_tests PRIVATE include)

    add_executable(aipb_explorer_fine_crossing_consensus_tests tests/engine_bridge_fine_crossing_consensus_tests.cpp)
    add_test(NAME aipb_explorer_fine_crossing_consensus COMMAND aipb_explorer_fine_crossing_consensus_tests)
    target_compile_features(aipb_explorer_fine_crossing_consensus_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_crossing_consensus_tests PRIVATE include)

    add_executable(aipb_explorer_fine_crossing_sectors_tests tests/engine_bridge_fine_crossing_sectors_tests.cpp)
    add_test(NAME aipb_explorer_fine_crossing_sectors COMMAND aipb_explorer_fine_crossing_sectors_tests)
    target_compile_features(aipb_explorer_fine_crossing_sectors_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_crossing_sectors_tests PRIVATE include)

    add_executable(aipb_explorer_fine_crossing_sequences_tests tests/engine_bridge_fine_crossing_sequences_tests.cpp)
    add_test(NAME aipb_explorer_fine_crossing_sequences COMMAND aipb_explorer_fine_crossing_sequences_tests)
    target_compile_features(aipb_explorer_fine_crossing_sequences_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_crossing_sequences_tests PRIVATE include)

    add_executable(aipb_explorer_fine_sequence_pairs_tests tests/engine_bridge_fine_sequence_pairs_tests.cpp)
    add_test(NAME aipb_explorer_fine_sequence_pairs COMMAND aipb_explorer_fine_sequence_pairs_tests)
    target_compile_features(aipb_explorer_fine_sequence_pairs_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_sequence_pairs_tests PRIVATE include)

    add_executable(aipb_explorer_fine_pair_sectors_tests tests/engine_bridge_fine_pair_sectors_tests.cpp)
    add_test(NAME aipb_explorer_fine_pair_sectors COMMAND aipb_explorer_fine_pair_sectors_tests)
    target_compile_features(aipb_explorer_fine_pair_sectors_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_pair_sectors_tests PRIVATE include)

    add_executable(aipb_explorer_fine_triplet_sectors_tests tests/engine_bridge_fine_triplet_sectors_tests.cpp)
    add_test(NAME aipb_explorer_fine_triplet_sectors COMMAND aipb_explorer_fine_triplet_sectors_tests)
    target_compile_features(aipb_explorer_fine_triplet_sectors_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_triplet_sectors_tests PRIVATE include)

    add_executable(aipb_explorer_fine_sequence_coverage_tests tests/engine_bridge_fine_sequence_coverage_tests.cpp)
    add_test(NAME aipb_explorer_fine_sequence_coverage COMMAND aipb_explorer_fine_sequence_coverage_tests)
    target_compile_features(aipb_explorer_fine_sequence_coverage_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_sequence_coverage_tests PRIVATE include)

    add_executable(aipb_explorer_fine_zero_reasons_tests tests/engine_bridge_fine_zero_reasons_tests.cpp)
    add_test(NAME aipb_explorer_fine_zero_reasons COMMAND aipb_explorer_fine_zero_reasons_tests)
    target_compile_features(aipb_explorer_fine_zero_reasons_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_zero_reasons_tests PRIVATE include)

    add_executable(aipb_explorer_fine_targets_tests tests/engine_bridge_fine_targets_tests.cpp)
    add_test(NAME aipb_explorer_fine_targets COMMAND aipb_explorer_fine_targets_tests)
    target_compile_features(aipb_explorer_fine_targets_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_targets_tests PRIVATE include)

    add_executable(aipb_explorer_fine_refinement_tests tests/engine_bridge_fine_refinement_tests.cpp)
    add_test(NAME aipb_explorer_fine_refinement COMMAND aipb_explorer_fine_refinement_tests)
    target_compile_features(aipb_explorer_fine_refinement_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_refinement_tests PRIVATE include)

    add_executable(aipb_explorer_fine_trial_gaps_tests tests/engine_bridge_fine_trial_gaps_tests.cpp)
    add_test(NAME aipb_explorer_fine_trial_gaps COMMAND aipb_explorer_fine_trial_gaps_tests)
    target_compile_features(aipb_explorer_fine_trial_gaps_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_trial_gaps_tests PRIVATE include)

    add_executable(aipb_explorer_fine_trial_plans_tests tests/engine_bridge_fine_trial_plans_tests.cpp)
    add_test(NAME aipb_explorer_fine_trial_plans COMMAND aipb_explorer_fine_trial_plans_tests)
    target_compile_features(aipb_explorer_fine_trial_plans_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_fine_trial_plans_tests PRIVATE include)

    add_executable(aipb_explorer_experiment_tests tests/engine_bridge_experiment_tests.cpp)
    add_test(NAME aipb_explorer_experiment COMMAND aipb_explorer_experiment_tests)
    target_compile_features(aipb_explorer_experiment_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_experiment_tests PRIVATE include)

    add_executable(aipb_explorer_scenario_tests tests/engine_bridge_scenario_tests.cpp)
    add_test(NAME aipb_explorer_scenario COMMAND aipb_explorer_scenario_tests)
    target_compile_features(aipb_explorer_scenario_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_scenario_tests PRIVATE include)

    add_executable(aipb_explorer_scenario_loader_tests tests/engine_bridge_scenario_loader_tests.cpp)
    add_test(NAME aipb_explorer_scenario_loader COMMAND aipb_explorer_scenario_loader_tests)
    target_compile_features(aipb_explorer_scenario_loader_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_scenario_loader_tests PRIVATE include)

    add_executable(aipb_explorer_bridge_math_tests tests/engine_bridge_math_tests.cpp)
    add_test(NAME aipb_explorer_bridge_math COMMAND aipb_explorer_bridge_math_tests)
    target_compile_features(aipb_explorer_bridge_math_tests PRIVATE cxx_std_17)
    target_include_directories(aipb_explorer_bridge_math_tests PRIVATE include)
endif()

set(aipb_explorer_targets aipb_explorer_core aipb_explorer_goldsrc)
if(BUILD_TESTING)
    list(APPEND aipb_explorer_targets aipb_explorer_contracts_tests aipb_explorer_environment_tests aipb_explorer_goldsrc_tests aipb_explorer_bridge_math_tests aipb_explorer_bridge_trace_tests aipb_explorer_named_trace_io_tests aipb_explorer_queue_tests aipb_explorer_trace_report_tests aipb_explorer_trace_classification_tests aipb_explorer_trace_observations_tests aipb_explorer_observation_collection_tests aipb_explorer_spatial_index_tests aipb_explorer_area_statistics_tests aipb_explorer_direction_evidence_tests aipb_explorer_mixed_evidence_audit_tests aipb_explorer_direction_assessment_tests aipb_explorer_trial_corroboration_tests aipb_explorer_source_consensus_tests aipb_explorer_spatial_localization_tests aipb_explorer_spatial_regions_tests aipb_explorer_cross_cell_regions_tests aipb_explorer_boundary_crossings_tests aipb_explorer_crossing_discovery_tests aipb_explorer_crossing_directions_tests aipb_explorer_directed_crossing_consensus_tests aipb_explorer_crossing_sectors_tests aipb_explorer_fine_crossings_tests aipb_explorer_fine_crossing_consensus_tests aipb_explorer_fine_crossing_sectors_tests aipb_explorer_fine_crossing_sequences_tests aipb_explorer_fine_sequence_pairs_tests aipb_explorer_fine_pair_sectors_tests aipb_explorer_fine_triplet_sectors_tests aipb_explorer_fine_sequence_coverage_tests aipb_explorer_fine_zero_reasons_tests aipb_explorer_fine_targets_tests aipb_explorer_fine_refinement_tests aipb_explorer_fine_trial_gaps_tests aipb_explorer_fine_trial_plans_tests aipb_explorer_experiment_tests aipb_explorer_scenario_tests aipb_explorer_scenario_loader_tests)
endif()
foreach(target IN LISTS aipb_explorer_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
