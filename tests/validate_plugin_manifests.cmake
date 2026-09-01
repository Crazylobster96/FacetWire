cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED FACETWIRE_SOURCE_DIR)
    message(FATAL_ERROR "FACETWIRE_SOURCE_DIR is required")
endif()

function(validate_manifest relative_path expected_plugin_id
        expected_registration)
    set(expected_capabilities ${ARGN})
    set(manifest_path "${FACETWIRE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${manifest_path}")
        message(FATAL_ERROR "Plugin manifest is missing: ${relative_path}")
    endif()

    file(READ "${manifest_path}" manifest)
    string(JSON format GET "${manifest}" format)
    string(JSON format_version GET "${manifest}" formatVersion)
    string(JSON plugin_id GET "${manifest}" plugin id)
    string(JSON abi_major GET "${manifest}" abi major)
    string(JSON artifact_profile GET "${manifest}" artifacts 0 profile)
    string(JSON registration GET "${manifest}" artifacts 0 registration)
    string(JSON actual_capability_count LENGTH "${manifest}" capabilities)
    string(JSON permission_count LENGTH "${manifest}" permissions)

    if(NOT format STREQUAL "facetwire.plugin-manifest")
        message(FATAL_ERROR "${relative_path}: unexpected format ${format}")
    endif()
    if(NOT format_version STREQUAL "0.1")
        message(FATAL_ERROR
            "${relative_path}: unexpected format version ${format_version}")
    endif()
    if(NOT plugin_id STREQUAL expected_plugin_id)
        message(FATAL_ERROR
            "${relative_path}: plugin ID ${plugin_id} does not match "
            "${expected_plugin_id}")
    endif()
    if(NOT abi_major EQUAL 1)
        message(FATAL_ERROR "${relative_path}: expected ABI major 1")
    endif()
    if(NOT artifact_profile STREQUAL "static" OR
       NOT registration STREQUAL expected_registration)
        message(FATAL_ERROR
            "${relative_path}: invalid static registration ${registration}")
    endif()
    if(NOT permission_count EQUAL 0)
        message(FATAL_ERROR
            "${relative_path}: reference renderer must request no permissions")
    endif()

    list(LENGTH expected_capabilities expected_capability_count)
    if(NOT actual_capability_count EQUAL expected_capability_count)
        message(FATAL_ERROR
            "${relative_path}: expected ${expected_capability_count} "
            "capabilities, found ${actual_capability_count}")
    endif()
    if(expected_capability_count GREATER 0)
        math(EXPR last_capability "${expected_capability_count} - 1")
        foreach(index RANGE 0 ${last_capability})
            list(GET expected_capabilities ${index} expected_capability)
            string(JSON actual_capability GET
                "${manifest}" capabilities ${index} id)
            if(NOT actual_capability STREQUAL expected_capability)
                message(FATAL_ERROR
                    "${relative_path}: capability ${index} is "
                    "${actual_capability}, expected ${expected_capability}")
            endif()
        endforeach()
    endif()
endfunction()

validate_manifest("plugins/placeholder_renderer/facetwire.plugin.json"
    "org.facetwire.reference.placeholder-renderer"
    "facetwire_placeholder_renderer_plugin_query"
    "facetwire.renderer.placeholder")
validate_manifest("plugins/text_renderer/facetwire.plugin.json"
    "org.facetwire.reference.text-renderer"
    "facetwire_text_renderer_plugin_query" "facetwire.renderer.text")
validate_manifest("plugins/core_image_renderer/facetwire.plugin.json"
    "org.facetwire.reference.core-image-renderer"
    "facetwire_core_image_plugin_query" "facetwire.renderer.image"
    "facetwire.renderer.animated-image")
validate_manifest("plugins/core_media_renderer/facetwire.plugin.json"
    "org.facetwire.reference.core-media-renderer"
    "facetwire_core_media_plugin_query" "facetwire.renderer.video"
    "facetwire.renderer.audio")
validate_manifest("plugins/core_chart_renderer/facetwire.plugin.json"
    "org.facetwire.reference.core-chart-renderer"
    "facetwire_core_chart_plugin_query" "facetwire.renderer.chart")
validate_manifest("plugins/hierarchical_chart_renderer/facetwire.plugin.json"
    "org.facetwire.reference.hierarchical-chart-renderer"
    "facetwire_hierarchical_chart_plugin_query"
    "facetwire.renderer.hierarchical-chart")

file(READ
    "${FACETWIRE_SOURCE_DIR}/plugins/core_chart_renderer/facetwire.plugin.json"
    chart_manifest)
string(JSON chart_interface_count LENGTH
    "${chart_manifest}" capabilities 0 interfaces)
if(NOT chart_interface_count EQUAL 3)
    message(FATAL_ERROR
        "core chart manifest must publish renderer, element and presentation interfaces")
endif()
string(JSON chart_renderer_interface GET
    "${chart_manifest}" capabilities 0 interfaces 0 id)
string(JSON chart_element_interface GET
    "${chart_manifest}" capabilities 0 interfaces 1 id)
string(JSON chart_presentation_interface GET
    "${chart_manifest}" capabilities 0 interfaces 2 id)
if(NOT chart_renderer_interface STREQUAL "facetwire.renderer.chart.v1" OR
   NOT chart_element_interface STREQUAL
       "facetwire.renderer.chart.elements.v1" OR
   NOT chart_presentation_interface STREQUAL
       "facetwire.renderer.chart.presentation.v1")
    message(FATAL_ERROR
        "core chart manifest publishes unexpected interfaces: "
        "${chart_renderer_interface};${chart_element_interface};"
        "${chart_presentation_interface}")
endif()
validate_manifest("plugins/flow_layout/facetwire.plugin.json"
    "org.facetwire.reference.flow-layout"
    "facetwire_flow_layout_plugin_query" "facetwire.layout.flow")
