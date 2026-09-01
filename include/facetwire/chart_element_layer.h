/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_CHART_ELEMENT_LAYER_H
#define FACETWIRE_CHART_ELEMENT_LAYER_H

#include <facetwire/chart_renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_CHART_ELEMENT_INTERFACE_ID \
    "facetwire.renderer.chart.elements.v1"
#define FW_CHART_ELEMENT_INTERFACE_VERSION 1u
#define FW_CHART_ELEMENT_PART_ANY UINT32_MAX

typedef uint32_t fw_chart_element_role;
#define FW_CHART_ELEMENT_ROLE_ANY             0u
#define FW_CHART_ELEMENT_ROLE_CHART_ROOT      1u
#define FW_CHART_ELEMENT_ROLE_PLOT_AREA       2u
#define FW_CHART_ELEMENT_ROLE_GRID            3u
#define FW_CHART_ELEMENT_ROLE_AXIS_X          4u
#define FW_CHART_ELEMENT_ROLE_AXIS_Y          5u
#define FW_CHART_ELEMENT_ROLE_TITLE           6u
#define FW_CHART_ELEMENT_ROLE_CATEGORY_LABEL  7u
#define FW_CHART_ELEMENT_ROLE_LEGEND_ITEM     8u
#define FW_CHART_ELEMENT_ROLE_SERIES          9u
#define FW_CHART_ELEMENT_ROLE_DATUM           10u
#define FW_CHART_ELEMENT_ROLE_VALUE_LABEL     11u
#define FW_CHART_ELEMENT_ROLE_ANNOTATION      12u
/* Legend Composition Profile 0.1 roles are additive: existing numeric values
 * remain stable. A legend item is a movable sub-template whose marker, label,
 * and optional value can also receive narrower overrides. */
#define FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER 13u
#define FW_CHART_ELEMENT_ROLE_LEGEND_MARKER    14u
#define FW_CHART_ELEMENT_ROLE_LEGEND_LABEL     15u
#define FW_CHART_ELEMENT_ROLE_LEGEND_VALUE     16u
#define FW_CHART_ELEMENT_ROLE_MAX              16u

typedef uint32_t fw_chart_element_capabilities;
#define FW_CHART_ELEMENT_CAN_HIDE       (1u << 0)
#define FW_CHART_ELEMENT_CAN_OPACITY    (1u << 1)
#define FW_CHART_ELEMENT_CAN_COLOR      (1u << 2)
#define FW_CHART_ELEMENT_CAN_TRANSFORM  (1u << 3)
#define FW_CHART_ELEMENT_CAN_REORDER    (1u << 4)
#define FW_CHART_ELEMENT_CAN_PROMOTE    (1u << 5)
#define FW_CHART_ELEMENT_DATA_BOUND     (1u << 6)

typedef uint32_t fw_chart_element_descriptor_flags;
#define FW_CHART_ELEMENT_BOUNDS_APPROXIMATE (1u << 0)
#define FW_CHART_ELEMENT_VIRTUALIZED        (1u << 1)

typedef uint32_t fw_chart_element_override_fields;
#define FW_CHART_OVERRIDE_VISIBLE     (UINT32_C(1) << 0)
#define FW_CHART_OVERRIDE_OPACITY     (UINT32_C(1) << 1)
#define FW_CHART_OVERRIDE_COLOR       (UINT32_C(1) << 2)
#define FW_CHART_OVERRIDE_TRANSLATION (UINT32_C(1) << 3)
#define FW_CHART_OVERRIDE_SCALE       (UINT32_C(1) << 4)
#define FW_CHART_OVERRIDE_ROTATION    (UINT32_C(1) << 5)
#define FW_CHART_OVERRIDE_ANCHOR      (UINT32_C(1) << 6)
#define FW_CHART_OVERRIDE_Z_OFFSET    (UINT32_C(1) << 7)
#define FW_CHART_OVERRIDE_PROMOTION   (UINT32_C(1) << 8)
#define FW_CHART_OVERRIDE_ALL         ((UINT32_C(1) << 9) - 1u)

typedef uint32_t fw_chart_element_promotion;
#define FW_CHART_ELEMENT_INLINE   0u
#define FW_CHART_ELEMENT_PROMOTED 1u

typedef struct fw_chart_element_ref_v1 {
    uint32_t struct_size;
    fw_chart_element_role role;
    fw_string_view chart_id;
    fw_string_view series_id;
    fw_string_view category_id;
    uint32_t part_index;
    uint32_t flags;
} fw_chart_element_ref_v1;

typedef struct fw_chart_element_descriptor_v1 {
    uint32_t struct_size;
    fw_chart_element_ref_v1 ref;
    fw_chart_element_ref_v1 parent;
    fw_rect_f32 normalized_bounds;
    int32_t z_index;
    fw_chart_element_capabilities capabilities;
    fw_string_view label;
    fw_chart_element_descriptor_flags flags;
} fw_chart_element_descriptor_v1;

typedef struct fw_chart_element_override_v1 {
    uint32_t struct_size;
    fw_chart_element_ref_v1 selector;
    fw_chart_element_override_fields fields;
    uint32_t visible;
    float opacity;
    fw_color_rgba_f32 color;
    fw_point_f32 translation;
    float uniform_scale;
    float rotation_radians;
    fw_point_f32 anchor;
    int32_t z_offset;
    fw_chart_element_promotion promotion;
    uint32_t flags;
} fw_chart_element_override_v1;

typedef struct fw_chart_element_presentation_v1 {
    uint32_t struct_size;
    uint32_t visible;
    float opacity;
    fw_color_rgba_f32 color;
    uint32_t has_color_override;
    fw_point_f32 translation;
    float uniform_scale;
    float rotation_radians;
    fw_point_f32 anchor;
    uint32_t has_explicit_anchor;
    int32_t z_offset;
    fw_chart_element_promotion promotion;
    uint32_t flags;
} fw_chart_element_presentation_v1;

typedef struct fw_chart_element_enum_sink_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *visit)(void *,
        const fw_chart_element_descriptor_v1 *);
} fw_chart_element_enum_sink_v1;

typedef struct fw_chart_element_enum_result_v1 {
    uint32_t struct_size;
    uint32_t emitted_element_count;
    uint32_t promotable_element_count;
    uint32_t virtualized_element_count;
    uint32_t flags;
} fw_chart_element_enum_result_v1;

typedef struct fw_chart_element_observer_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *select)(void *,
        const fw_chart_element_descriptor_v1 *,
        const fw_chart_element_presentation_v1 *);
} fw_chart_element_observer_v1;

typedef struct fw_chart_element_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate_overrides)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        const fw_chart_element_override_v1 *, size_t,
        fw_chart_validation_result_v1 *);
    fw_status(FW_CALL *enumerate)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        const fw_chart_element_enum_sink_v1 *,
        fw_chart_element_enum_result_v1 *);
    fw_status(FW_CALL *format_element_id)(fw_plugin_handle,
        const fw_chart_element_ref_v1 *, char *, size_t, size_t *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_chart_renderer_request_v1 *,
        const fw_chart_element_override_v1 *, size_t,
        fw_rect_f32, const fw_chart_services_v1 *,
        const fw_chart_element_observer_v1 *,
        fw_chart_render_result_v1 *);
} fw_chart_element_api_v1;

#ifdef __cplusplus
}
#endif

#endif
