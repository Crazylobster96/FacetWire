/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_CHART_LEGEND_H
#define FACETWIRE_CHART_LEGEND_H

#include <facetwire/chart_presentation.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Legend Composition Profile 0.1 is a request-scoped, host-owned model.
 * Renderers must not retain pointers contained by these structures. */
#define FW_CHART_LEGEND_PROFILE_VERSION 1u

typedef uint32_t fw_chart_legend_direction;
#define FW_CHART_LEGEND_DIRECTION_AUTO   0u
#define FW_CHART_LEGEND_DIRECTION_ROW    1u
#define FW_CHART_LEGEND_DIRECTION_COLUMN 2u

typedef uint32_t fw_chart_legend_wrap;
#define FW_CHART_LEGEND_WRAP_AUTO 0u
#define FW_CHART_LEGEND_WRAP_NONE 1u
#define FW_CHART_LEGEND_WRAP_ITEMS 2u

typedef uint32_t fw_chart_legend_alignment;
#define FW_CHART_LEGEND_ALIGN_START  0u
#define FW_CHART_LEGEND_ALIGN_CENTER 1u
#define FW_CHART_LEGEND_ALIGN_END    2u

typedef uint32_t fw_chart_legend_marker_shape;
#define FW_CHART_LEGEND_MARKER_AUTO   0u
#define FW_CHART_LEGEND_MARKER_SQUARE 1u
#define FW_CHART_LEGEND_MARKER_CIRCLE 2u
#define FW_CHART_LEGEND_MARKER_LINE   3u
#define FW_CHART_LEGEND_MARKER_AREA   4u

typedef uint32_t fw_chart_legend_item_state;
#define FW_CHART_LEGEND_ITEM_NORMAL      0u
#define FW_CHART_LEGEND_ITEM_HIGHLIGHTED 1u
#define FW_CHART_LEGEND_ITEM_MUTED       2u
#define FW_CHART_LEGEND_ITEM_DISABLED    3u

typedef uint32_t fw_chart_legend_flags;
#define FW_CHART_LEGEND_SHOW_VALUES       (UINT32_C(1) << 0)
#define FW_CHART_LEGEND_INTERACTIVE       (UINT32_C(1) << 1)
#define FW_CHART_LEGEND_ALLOW_ITEM_WRAP   (UINT32_C(1) << 2)
#define FW_CHART_LEGEND_RESERVE_EMPTY     (UINT32_C(1) << 3)

/* All dimensions use the chart's normalized [0,1] coordinate space. */
typedef struct fw_chart_legend_tokens_v1 {
    uint32_t struct_size;
    float marker_size;
    float marker_label_gap;
    float label_value_gap;
    float item_gap;
    float row_gap;
    float item_padding_x;
    float item_padding_y;
    float min_item_width;
    float max_item_width;
    float label_font_size;
    float value_font_size;
    uint32_t flags;
} fw_chart_legend_tokens_v1;

typedef struct fw_chart_legend_item_v1 {
    uint32_t struct_size;
    fw_string_view id;
    fw_string_view series_id;
    fw_string_view label;
    fw_string_view value_text;
    fw_string_view semantic_label;
    fw_color_rgba_f32 color;
    fw_chart_legend_marker_shape marker_shape;
    fw_chart_legend_item_state state;
    uint32_t visible;
    uint32_t flags;
} fw_chart_legend_item_v1;

typedef struct fw_chart_legend_template_v1 {
    uint32_t struct_size;
    uint32_t profile_version;
    fw_string_view template_id;
    fw_chart_legend_placement placement;
    fw_chart_legend_direction direction;
    fw_chart_legend_wrap wrap;
    fw_chart_legend_alignment alignment;
    fw_chart_legend_tokens_v1 tokens;
    const fw_chart_legend_item_v1 *items;
    size_t item_count;
    fw_chart_legend_flags flags;
} fw_chart_legend_template_v1;

#ifdef __cplusplus
}
#endif

#endif
