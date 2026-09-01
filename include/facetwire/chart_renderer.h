/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_CHART_RENDERER_H
#define FACETWIRE_CHART_RENDERER_H

#include <facetwire/facetwire.h>
#include <facetwire/geometry.h>
#include <facetwire/renderer.h>
#include <facetwire/render_target.h>
#include <facetwire/semantics.h>
#include <facetwire/visual_transform.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_CHART_RENDERER_CAPABILITY_ID "facetwire.renderer.chart"
#define FW_CHART_RENDERER_INTERFACE_ID "facetwire.renderer.chart.v1"
#define FW_CHART_RENDERER_INTERFACE_VERSION 1u

typedef uint32_t fw_chart_kind;
#define FW_CHART_BAR  1u
#define FW_CHART_LINE 2u
#define FW_CHART_PIE  3u
#define FW_CHART_AREA        4u
#define FW_CHART_SCATTER     5u
#define FW_CHART_BUBBLE      6u
#define FW_CHART_DONUT       7u
#define FW_CHART_RADAR       8u
#define FW_CHART_HEATMAP     9u
#define FW_CHART_GAUGE      10u
#define FW_CHART_BOX_PLOT   11u
#define FW_CHART_HISTOGRAM  12u
#define FW_CHART_WATERFALL  13u
#define FW_CHART_FUNNEL     14u
#define FW_CHART_CANDLESTICK 15u
#define FW_CHART_TIME_SERIES 16u
#define FW_CHART_COMBO       17u
#define FW_CHART_DIVERGING_BAR  18u
#define FW_CHART_FACET_LINE     19u
#define FW_CHART_RANGE_AREA     20u
#define FW_CHART_DENSITY_HEATMAP 21u
#define FW_CHART_WORD_CLOUD     22u
#define FW_CHART_ROSE           23u

typedef uint32_t fw_chart_series_mark;
#define FW_CHART_MARK_AUTO    0u
#define FW_CHART_MARK_BAR     1u
#define FW_CHART_MARK_LINE    2u
#define FW_CHART_MARK_AREA    3u
#define FW_CHART_MARK_SCATTER 4u

typedef uint32_t fw_chart_orientation;
#define FW_CHART_ORIENTATION_VERTICAL   0u
#define FW_CHART_ORIENTATION_HORIZONTAL 1u

typedef uint32_t fw_chart_stack_mode;
#define FW_CHART_STACK_NONE    0u
#define FW_CHART_STACK_NORMAL  1u
#define FW_CHART_STACK_PERCENT 2u

typedef uint32_t fw_chart_value_label_mode;
#define FW_CHART_VALUE_LABEL_VALUE             0u
#define FW_CHART_VALUE_LABEL_PERCENT           1u
#define FW_CHART_VALUE_LABEL_VALUE_AND_PERCENT 2u

typedef uint32_t fw_chart_element_kind;
#define FW_CHART_ELEMENT_NONE       0u
#define FW_CHART_ELEMENT_BAR        1u
#define FW_CHART_ELEMENT_LINE_POINT 2u
#define FW_CHART_ELEMENT_PIE_SLICE  3u
#define FW_CHART_ELEMENT_POINT      4u
#define FW_CHART_ELEMENT_CELL       5u
#define FW_CHART_ELEMENT_RANGE      6u

typedef struct fw_chart_category_v1 {
    uint32_t struct_size;
    fw_string_view id;
    fw_string_view label;
    uint32_t flags;
} fw_chart_category_v1;

typedef struct fw_chart_value_v1 {
    uint32_t struct_size;
    double value;
    uint32_t missing;
    uint32_t flags;
    double x;
    double size;
    double minimum;
    double quartile1;
    double median;
    double quartile3;
    double maximum;
    double open;
    double high;
    double low;
    double close;
} fw_chart_value_v1;

typedef struct fw_chart_series_v1 {
    uint32_t struct_size;
    fw_string_view id;
    fw_string_view label;
    const fw_chart_value_v1 *values;
    size_t value_count;
    fw_color_rgba_f32 color;
    uint32_t visible;
    uint32_t flags;
    fw_chart_series_mark mark;
} fw_chart_series_v1;

typedef struct fw_chart_style_v1 {
    uint32_t struct_size;
    uint32_t show_axes;
    uint32_t show_grid;
    uint32_t show_legend;
    uint32_t show_labels;
    float bar_gap_ratio;
    float line_width;
    float point_radius;
    fw_color_rgba_f32 foreground;
    fw_color_rgba_f32 grid_color;
    uint32_t flags;
    uint32_t show_value_labels;
    fw_chart_value_label_mode value_label_mode;
    uint32_t value_precision;
    fw_chart_orientation orientation;
    fw_chart_stack_mode stack_mode;
    float fill_opacity;
    float donut_inner_radius;
} fw_chart_style_v1;

typedef struct fw_chart_budget_v1 {
    uint32_t struct_size;
    uint32_t max_categories;
    uint32_t max_series;
    uint32_t max_points;
    uint32_t max_commands;
    uint32_t flags;
} fw_chart_budget_v1;

typedef struct fw_chart_renderer_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_string_view zone_id;
    fw_string_view chart_id;
    fw_string_view title;
    fw_string_view summary;
    fw_chart_kind kind;
    const fw_chart_category_v1 *categories;
    size_t category_count;
    const fw_chart_series_v1 *series;
    size_t series_count;
    float opacity;
    fw_size_f32 intrinsic_size;
    fw_visual_transform_v1 transform;
    fw_chart_style_v1 style;
    fw_chart_budget_v1 budget;
    fw_layout_constraints_v1 constraints;
    fw_render_target_profile_v1 target;
    uint64_t presentation_revision;
    uint32_t flags;
} fw_chart_renderer_request_v1;

/* Chart primitives use normalized, unrotated chart coordinates in [0, 1].
 * begin_chart supplies the shared VisualTransform that maps that space into
 * the target viewport. The sink owns no pointers passed by the renderer. */
typedef struct fw_chart_draw_sink_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *begin_chart)(void *,
        const fw_visual_transform_result_v1 *, float opacity);
    fw_status(FW_CALL *end_chart)(void *);
    fw_status(FW_CALL *fill_rect)(void *, fw_rect_f32,
        fw_color_rgba_f32, fw_string_view series_id,
        fw_string_view category_id);
    fw_status(FW_CALL *stroke_line)(void *, fw_point_f32, fw_point_f32,
        float width, fw_color_rgba_f32, fw_string_view series_id,
        fw_string_view category_id);
    fw_status(FW_CALL *fill_circle)(void *, fw_point_f32, float radius,
        fw_color_rgba_f32, fw_string_view series_id,
        fw_string_view category_id);
    fw_status(FW_CALL *fill_sector)(void *, fw_point_f32,
        float outer_radius, float inner_radius, float start_radians,
        float sweep_radians, fw_color_rgba_f32,
        fw_string_view series_id, fw_string_view category_id);
    fw_status(FW_CALL *fill_polygon)(void *, const fw_point_f32 *points,
        size_t point_count, fw_color_rgba_f32,
        fw_string_view series_id, fw_string_view category_id);
    fw_status(FW_CALL *draw_label)(void *, fw_string_view text,
        fw_point_f32 anchor, float font_size, fw_color_rgba_f32,
        fw_string_view element_id);
} fw_chart_draw_sink_v1;

typedef struct fw_chart_services_v1 {
    uint32_t struct_size;
    const fw_chart_draw_sink_v1 *draw;
    uint32_t flags;
} fw_chart_services_v1;

typedef struct fw_chart_validation_result_v1 {
    uint32_t struct_size;
    fw_status status;
    uint32_t normalization_flags;
    fw_string_view diagnostic_key;
} fw_chart_validation_result_v1;

typedef struct fw_chart_measure_result_v1 {
    uint32_t struct_size;
    fw_size_f32 size;
    fw_size_f32 intrinsic_size;
    uint32_t flags;
} fw_chart_measure_result_v1;

typedef struct fw_chart_render_result_v1 {
    uint32_t struct_size;
    fw_visual_transform_result_v1 transform;
    uint32_t emitted_command_count;
    uint32_t rendered_series_count;
    uint32_t rendered_value_count;
    uint32_t uncovered_is_transparent;
    uint64_t cache_key_high;
    uint64_t cache_key_low;
    uint32_t flags;
} fw_chart_render_result_v1;

typedef struct fw_chart_semantics_v1 {
    uint32_t struct_size;
    uint32_t role;
    fw_string_view label;
    fw_string_view summary;
    fw_rect_f32 bounds;
    uint32_t series_count;
    uint32_t value_count;
    uint32_t flags;
} fw_chart_semantics_v1;

typedef struct fw_chart_hit_result_v1 {
    uint32_t struct_size;
    uint32_t hit;
    fw_chart_element_kind element_kind;
    uint32_t series_index;
    uint32_t value_index;
    fw_string_view series_id;
    fw_string_view category_id;
    double value;
    fw_point_f32 normalized_point;
    uint32_t flags;
} fw_chart_hit_result_v1;

typedef struct fw_chart_renderer_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        fw_chart_validation_result_v1 *);
    fw_status(FW_CALL *measure)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        fw_chart_measure_result_v1 *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *, fw_rect_f32,
        const fw_chart_services_v1 *, fw_chart_render_result_v1 *);
    fw_status(FW_CALL *build_semantics)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *, fw_rect_f32,
        fw_chart_semantics_v1 *);
    fw_status(FW_CALL *hit_test)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *, fw_rect_f32,
        fw_point_f32, fw_chart_hit_result_v1 *);
    fw_status(FW_CALL *get_parameter_schema)(fw_plugin_handle,
        fw_string_view *out_schema_json);
} fw_chart_renderer_api_v1;

#if defined(FACETWIRE_CORE_CHART_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_core_chart_plugin_query(fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
