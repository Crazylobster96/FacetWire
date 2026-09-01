/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_HIERARCHICAL_CHART_RENDERER_H
#define FACETWIRE_HIERARCHICAL_CHART_RENDERER_H

#include <facetwire/chart_renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_HIERARCHICAL_CHART_CAPABILITY_ID \
    "facetwire.renderer.hierarchical-chart"
#define FW_HIERARCHICAL_CHART_INTERFACE_ID \
    "facetwire.renderer.hierarchical-chart.v1"
#define FW_HIERARCHICAL_CHART_INTERFACE_VERSION 1u
#define FW_HIERARCHICAL_ROOT_INDEX UINT32_MAX

typedef uint32_t fw_hierarchical_chart_kind;
#define FW_HIERARCHICAL_CHART_TREEMAP       1u
#define FW_HIERARCHICAL_CHART_SUNBURST      2u
#define FW_HIERARCHICAL_CHART_PACKED_BUBBLE 3u

typedef struct fw_hierarchical_chart_node_v1 {
    uint32_t struct_size;
    fw_string_view id;
    fw_string_view label;
    uint32_t parent_index;
    double value;
    fw_color_rgba_f32 color;
    uint32_t visible;
    uint32_t flags;
} fw_hierarchical_chart_node_v1;

typedef struct fw_hierarchical_chart_style_v1 {
    uint32_t struct_size;
    uint32_t show_labels;
    uint32_t show_values;
    uint32_t max_visible_labels;
    float gap;
    float inner_radius;
    float label_scale;
    uint32_t flags;
} fw_hierarchical_chart_style_v1;

typedef struct fw_hierarchical_chart_budget_v1 {
    uint32_t struct_size;
    uint32_t max_nodes;
    uint32_t max_depth;
    uint32_t max_commands;
    uint32_t flags;
} fw_hierarchical_chart_budget_v1;

typedef struct fw_hierarchical_chart_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_string_view zone_id;
    fw_string_view chart_id;
    fw_string_view title;
    fw_string_view summary;
    fw_hierarchical_chart_kind kind;
    const fw_hierarchical_chart_node_v1 *nodes;
    size_t node_count;
    float opacity;
    fw_size_f32 intrinsic_size;
    fw_visual_transform_v1 transform;
    fw_hierarchical_chart_style_v1 style;
    fw_hierarchical_chart_budget_v1 budget;
    fw_layout_constraints_v1 constraints;
    fw_render_target_profile_v1 target;
    uint64_t presentation_revision;
    uint32_t flags;
} fw_hierarchical_chart_request_v1;

typedef struct fw_hierarchical_chart_hit_result_v1 {
    uint32_t struct_size;
    uint32_t hit;
    uint32_t node_index;
    fw_string_view node_id;
    double value;
    fw_point_f32 normalized_point;
    uint32_t flags;
} fw_hierarchical_chart_hit_result_v1;

typedef struct fw_hierarchical_chart_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_hierarchical_chart_request_v1 *,
        fw_chart_validation_result_v1 *);
    fw_status(FW_CALL *measure)(fw_plugin_handle,
        const fw_hierarchical_chart_request_v1 *,
        fw_chart_measure_result_v1 *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_hierarchical_chart_request_v1 *, fw_rect_f32,
        const fw_chart_services_v1 *, fw_chart_render_result_v1 *);
    fw_status(FW_CALL *build_semantics)(fw_plugin_handle,
        const fw_hierarchical_chart_request_v1 *, fw_rect_f32,
        fw_chart_semantics_v1 *);
    fw_status(FW_CALL *hit_test)(fw_plugin_handle,
        const fw_hierarchical_chart_request_v1 *, fw_rect_f32,
        fw_point_f32, fw_hierarchical_chart_hit_result_v1 *);
    fw_status(FW_CALL *get_parameter_schema)(fw_plugin_handle,
        fw_string_view *out_schema_json);
} fw_hierarchical_chart_api_v1;

#if defined(FACETWIRE_HIERARCHICAL_CHART_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_hierarchical_chart_plugin_query(fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
