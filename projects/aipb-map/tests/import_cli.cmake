# Exercise the public CLI against the synthetic GoldSrc BSP fixture.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(BSP "${WORK}/fixture.bsp")
set(MAP "${WORK}/fixture.aipbmap")
set(LOG "${WORK}/import.log")

execute_process(COMMAND "${FIXTURE}" --write-cli-fixture "${BSP}"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Failed to generate BSP fixture: ${error}")
endif()

execute_process(COMMAND "${CLI}" import --format bsp --input "${BSP}" --output "${MAP}" --log-file "${LOG}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT output MATCHES "Imported fixture:")
    message(FATAL_ERROR "CLI import failed: ${status}: ${output}: ${error}")
endif()

if(NOT EXISTS "${LOG}")
    message(FATAL_ERROR "CLI import did not create a diagnostic log")
endif()
file(READ "${LOG}" logtext)
if(NOT logtext MATCHES "Import completed successfully")
    message(FATAL_ERROR "CLI import log lacks completion marker: ${logtext}")
endif()

if(NOT EXISTS "${MAP}/manifest.json")
    message(FATAL_ERROR "CLI import did not produce the map artifact")
endif()

execute_process(COMMAND "${CLI}" inspect --input "${MAP}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT output MATCHES "Map: fixture")
    message(FATAL_ERROR "Imported artifact cannot be inspected: ${status}: ${output}: ${error}")
endif()

execute_process(COMMAND "${CLI}" import --format bsp --input "${BSP}" --output "${MAP}"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0)
    message(FATAL_ERROR "CLI import overwrote an existing artifact")
endif()

## End of previous cases

# Empty render brush models must not abort import or lose collision links.
set(EMPTY_BSP "${WORK}/empty-render.bsp")
set(EMPTY_MAP "${WORK}/empty-render.aipbmap")
set(EMPTY_LOG "${WORK}/empty-render.log")
execute_process(COMMAND "${FIXTURE}" --write-empty-render-fixture "${EMPTY_BSP}"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Failed to generate empty-render BSP: ${error}")
endif()
execute_process(COMMAND "${CLI}" import --format bsp --input "${EMPTY_BSP}" --output "${EMPTY_MAP}" --log-file "${EMPTY_LOG}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Empty-render import failed: ${status}: ${output}: ${error}")
endif()
file(READ "${EMPTY_LOG}" empty_log)
if(NOT empty_log MATCHES "model \\*1: no render faces")
    message(FATAL_ERROR "Empty-render diagnostic missing: ${empty_log}")
endif()
execute_process(COMMAND "${CLI}" inspect --input "${EMPTY_MAP}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Empty-render artifact cannot be inspected: ${error}")
endif()
file(REMOVE_RECURSE "${WORK}")
