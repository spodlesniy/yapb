# Validate deterministic collision report and preserve the input artifact.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(MAP "${WORK}/fixture.aipbmap")
set(REPORT "${WORK}/validation.json")
set(LOG "${WORK}/collision-validation.log")
execute_process(COMMAND "${FIXTURE}" --write-fixture "${MAP}"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Fixture creation failed: ${error}")
endif()
file(SHA256 "${MAP}/manifest.json" before)
execute_process(COMMAND "${CLI}" validate-collision --log-file "${LOG}" --output "${REPORT}" --input "${MAP}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT output MATCHES "violations=0")
    message(FATAL_ERROR "Validation failed: ${status}: ${output}: ${error}")
endif()
if(NOT EXISTS "${LOG}")
    message(FATAL_ERROR "Validation log was not created")
endif()
file(READ "${LOG}" logtext)
foreach(expected IN ITEMS "Collision validation started" "Source SHA-256:"
        "Artifact SHA-256:" "World hull 0:" "Brush hull 3:"
        "Collision validation report JSON:" "violations=0"
        "Collision validation completed successfully")
    if(NOT logtext MATCHES "${expected}")
        message(FATAL_ERROR "Validation log missing ${expected}: ${logtext}")
    endif()
endforeach()
file(READ "${REPORT}" json)
string(JSON schema GET "${json}" schema_version)
string(JSON count GET "${json}" model_count)
string(JSON violations GET "${json}" violations)
string(JSON samples LENGTH "${json}" samples)
if(NOT schema STREQUAL "0.1.0" OR NOT count EQUAL 2 OR
   NOT violations EQUAL 0 OR NOT samples EQUAL 8)
    message(FATAL_ERROR "Unexpected validation JSON: ${json}")
endif()
execute_process(COMMAND "${CLI}" validate-collision --input "${MAP}" --output "${REPORT}"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0)
    message(FATAL_ERROR "Validation overwrote existing report")
endif()
execute_process(COMMAND "${CLI}" validate-collision --input "${MAP}" --output "${MAP}/inside.json"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0 OR EXISTS "${MAP}/inside.json")
    message(FATAL_ERROR "Validation wrote inside source artifact")
endif()
execute_process(COMMAND "${CLI}" validate-collision --input "${MAP}" --output "${WORK}/duplicate.json"
    --log-file "${LOG}" --log-file "${LOG}"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0)
    message(FATAL_ERROR "Validation accepted duplicated log option")
endif()
execute_process(COMMAND "${CLI}" validate-collision --input "${MAP}" --output "${WORK}/inside-log.json"
    --log-file "${MAP}/inside.log"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0 OR EXISTS "${MAP}/inside.log")
    message(FATAL_ERROR "Validation accepted log path inside artifact")
endif()
execute_process(COMMAND "${CLI}" validate-collision --input "${MAP}" --output "${WORK}/legacy.json"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT output MATCHES "violations=0")
    message(FATAL_ERROR "Validation without log failed: ${output}: ${error}")
endif()
file(READ "${WORK}/legacy.json" legacy)
if(NOT legacy STREQUAL json)
    message(FATAL_ERROR "Log option changed deterministic validation report")
endif()
file(SHA256 "${MAP}/manifest.json" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "Validation modified map artifact")
endif()
file(REMOVE_RECURSE "${WORK}")
