# SPDX-License-Identifier: MPL-2.0
# Exercise a non-ANSI path even when the checkout/build directory is ASCII.
if(NOT EXISTS "${DISCOVERY_EXE}" OR NOT EXISTS "${PLUGIN_FILE}")
    message(FATAL_ERROR "Built discovery executable and plugin are required")
endif()
set(unicode_dir "${STAGING_DIR}/中文路径-Ω-🚀")
file(MAKE_DIRECTORY "${unicode_dir}")
set(unicode_plugin "${unicode_dir}/动态插件.dll")
file(COPY_FILE "${PLUGIN_FILE}" "${unicode_plugin}" ONLY_IF_DIFFERENT)
execute_process(
    COMMAND "${DISCOVERY_EXE}" "${unicode_plugin}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 15)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Unicode discovery failed (${result}): ${output}${error}")
endif()
