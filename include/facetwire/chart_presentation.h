/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_CHART_PRESENTATION_H
#define FACETWIRE_CHART_PRESENTATION_H

#include <facetwire/chart_element_layer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_CHART_PRESENTATION_INTERFACE_ID \
    "facetwire.renderer.chart.presentation.v1"
#define FW_CHART_PRESENTATION_INTERFACE_VERSION 1u

typedef uint32_t fw_chart_theme;
#define FW_CHART_THEME_AUTO          0u
#define FW_CHART_THEME_LIGHT         1u
#define FW_CHART_THEME_DARK          2u
#define FW_CHART_THEME_BUSINESS      3u
#define FW_CHART_THEME_ACADEMIC      4u
#define FW_CHART_THEME_HIGH_CONTRAST 5u

typedef uint32_t fw_chart_legend_placement;
#define FW_CHART_LEGEND_AUTO   0u
#define FW_CHART_LEGEND_BOTTOM 1u
#define FW_CHART_LEGEND_RIGHT  2u
#define FW_CHART_LEGEND_HIDDEN 3u

typedef uint32_t fw_chart_label_policy;
#define FW_CHART_LABEL_AUTO      0u
#define FW_CHART_LABEL_ALL       1u
#define FW_CHART_LABEL_IMPORTANT 2u
#define FW_CHART_LABEL_NONE      3u

#define FW_CHART_PRESENTATION_USE_THEME_PALETTE (1u << 0)
#define FW_CHART_PRESENTATION_AVOID_COLLISIONS   (1u << 1)
#define FW_CHART_PRESENTATION_DIRECT_LABELS      (1u << 2)

typedef struct fw_chart_presentation_v1 {
    uint32_t struct_size;
    fw_chart_theme theme;
    fw_chart_legend_placement legend_placement;
    fw_chart_label_policy label_policy;
    uint32_t auto_layout;
    uint32_t max_visible_labels;
    float label_padding;
    float title_scale;
    float label_scale;
    float value_scale;
    uint32_t flags;
} fw_chart_presentation_v1;

typedef struct fw_chart_presentation_plan_v1 {
    uint32_t struct_size;
    fw_chart_theme resolved_theme;
    fw_chart_legend_placement resolved_legend_placement;
    uint32_t category_label_stride;
    uint32_t value_label_stride;
    uint32_t max_visible_labels;
    uint32_t flags;
} fw_chart_presentation_plan_v1;

typedef struct fw_chart_presentation_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        const fw_chart_presentation_v1 *,
        fw_chart_validation_result_v1 *);
    fw_status(FW_CALL *resolve)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        const fw_chart_presentation_v1 *,
        fw_chart_presentation_plan_v1 *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        const fw_chart_presentation_v1 *,
        const fw_chart_element_override_v1 *, size_t,
        fw_rect_f32, const fw_chart_services_v1 *,
        const fw_chart_element_observer_v1 *,
        fw_chart_render_result_v1 *);
} fw_chart_presentation_api_v1;

#ifdef __cplusplus
}
#endif

#endif
