if(NOT IS_ABSOLUTE "${RENDERER_TEST_EXE}" OR
   NOT IS_ABSOLUTE "${RENDERER_PLUGIN_FILE}" OR
   NOT IS_ABSOLUTE "${RENDERER_STAGING_DIR}")
    message(FATAL_ERROR "Exact absolute renderer test paths required")
endif()
if(NOT EXISTS "${RENDERER_TEST_EXE}" OR
   NOT EXISTS "${RENDERER_PLUGIN_FILE}")
    message(FATAL_ERROR "Renderer test binary or plugin missing")
endif()
file(MAKE_DIRECTORY "${RENDERER_STAGING_DIR}")
get_filename_component(renderer_name "${RENDERER_PLUGIN_FILE}" NAME)
set(staged_plugin "${RENDERER_STAGING_DIR}/${renderer_name}")
file(COPY_FILE "${RENDERER_PLUGIN_FILE}" "${staged_plugin}" ONLY_IF_DIFFERENT)
execute_process(
    COMMAND "${RENDERER_TEST_EXE}" "${staged_plugin}"
        "${RENDERER_ID}" "${RENDERER_CAPABILITY}" "${RENDERER_INTERFACE}"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Isolated renderer load failed: ${result}")
endif()
