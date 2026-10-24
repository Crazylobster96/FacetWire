if(NOT IS_ABSOLUTE "${RENDERER_TEST_EXE}" OR
   NOT IS_ABSOLUTE "${RENDERER_PLUGIN_FILE}" OR
   NOT IS_ABSOLUTE "${RENDERER_STAGING_DIR}" OR
   NOT IS_ABSOLUTE "${RENDERER_MANIFEST_FILE}")
    message(FATAL_ERROR "Exact absolute renderer test paths required")
endif()
if(NOT EXISTS "${RENDERER_TEST_EXE}" OR
   NOT EXISTS "${RENDERER_PLUGIN_FILE}" OR
   NOT EXISTS "${RENDERER_MANIFEST_FILE}")
    message(FATAL_ERROR "Renderer test binary or plugin missing")
endif()
file(MAKE_DIRECTORY "${RENDERER_STAGING_DIR}")
get_filename_component(renderer_name "${RENDERER_PLUGIN_FILE}" NAME)
set(staged_plugin "${RENDERER_STAGING_DIR}/${renderer_name}")
file(COPY_FILE "${RENDERER_PLUGIN_FILE}" "${staged_plugin}" ONLY_IF_DIFFERENT)
file(READ "${RENDERER_MANIFEST_FILE}" manifest)
string(JSON plugin_id GET "${manifest}" plugin id)
string(JSON capability_count LENGTH "${manifest}" capabilities)
if(NOT plugin_id STREQUAL RENDERER_ID OR capability_count LESS 1)
    message(FATAL_ERROR "Renderer manifest identity/capabilities changed")
endif()
math(EXPR last_capability "${capability_count} - 1")
foreach(index RANGE 0 ${last_capability})
    string(JSON capability GET "${manifest}" capabilities ${index} id)
    string(JSON flags GET "${manifest}" capabilities ${index} flags)
    string(JSON interface_count LENGTH "${manifest}" capabilities ${index} interfaces)
    if(interface_count LESS 1)
        message(FATAL_ERROR "Renderer capability lacks a queryable interface")
    endif()
    string(JSON interface_id GET "${manifest}" capabilities ${index} interfaces 0 id)
    execute_process(
        COMMAND "${RENDERER_TEST_EXE}" "${staged_plugin}"
            "${RENDERER_ID}" "${capability}" "${interface_id}"
            "${flags}" "${capability_count}"
        RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Isolated renderer capability ${capability} mismatches manifest: ${result}")
    endif()
endforeach()
