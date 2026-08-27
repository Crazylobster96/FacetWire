/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_PLACEHOLDER_RENDERER_H
#define FACETWIRE_PLACEHOLDER_RENDERER_H

#include <facetwire/display_list.h>
#include <facetwire/renderer.h>
#include <facetwire/semantics.h>
#include <facetwire/text_service.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_PLACEHOLDER_RENDERER_INTERFACE_ID \
    "facetwire.renderer.placeholder.v1"
#define FW_PLACEHOLDER_RENDERER_INTERFACE_VERSION 1u
#define FW_PLACEHOLDER_RENDERER_CAPABILITY_ID \
    "facetwire.renderer.placeholder"

typedef uint32_t fw_placeholder_reason;
#define FW_PLACEHOLDER_REASON_LOADING               1u
#define FW_PLACEHOLDER_REASON_RENDERER_MISSING      2u
#define FW_PLACEHOLDER_REASON_UNSUPPORTED_TYPE      3u
#define FW_PLACEHOLDER_REASON_RESOURCE_MISSING      4u
#define FW_PLACEHOLDER_REASON_RESOURCE_UNAVAILABLE  5u
#define FW_PLACEHOLDER_REASON_PARSE_FAILED          6u
#define FW_PLACEHOLDER_REASON_DECODE_FAILED         7u
#define FW_PLACEHOLDER_REASON_POLICY_BLOCKED        8u
#define FW_PLACEHOLDER_REASON_PERMISSION_REQUIRED   9u
#define FW_PLACEHOLDER_REASON_RESOURCE_LIMITED     10u
#define FW_PLACEHOLDER_REASON_PLUGIN_FAILED        11u
#define FW_PLACEHOLDER_REASON_UNKNOWN              12u

typedef uint32_t fw_placeholder_mode;
#define FW_PLACEHOLDER_MODE_HIDDEN     1u
#define FW_PLACEHOLDER_MODE_MINIMAL    2u
#define FW_PLACEHOLDER_MODE_STANDARD   3u
#define FW_PLACEHOLDER_MODE_DIAGNOSTIC 4u

typedef uint32_t fw_placeholder_action_mask;
#define FW_PLACEHOLDER_ACTION_NONE               0u
#define FW_PLACEHOLDER_ACTION_RETRY        (1u << 0)
#define FW_PLACEHOLDER_ACTION_SHOW_DETAILS (1u << 1)
#define FW_PLACEHOLDER_ACTION_LOCATE       (1u << 2)
#define FW_PLACEHOLDER_ACTION_PERMISSION   (1u << 3)
#define FW_PLACEHOLDER_ACTION_FIND_PLUGIN  (1u << 4)
#define FW_PLACEHOLDER_ACTION_ALTERNATIVE  (1u << 5)
#define FW_PLACEHOLDER_ACTION_ALL          ((1u << 6) - 1u)

typedef uint32_t fw_placeholder_normalization_flags;
#define FW_PH_NORMALIZED_NONE         0u
#define FW_PH_NORMALIZED_REASON       (1u << 0)
#define FW_PH_NORMALIZED_MODE         (1u << 1)
#define FW_PH_NORMALIZED_CONSTRAINTS  (1u << 2)
#define FW_PH_NORMALIZED_INTRINSIC    (1u << 3)
#define FW_PH_NORMALIZED_STYLE        (1u << 4)
#define FW_PH_NORMALIZED_TEXT         (1u << 5)
#define FW_PH_NORMALIZED_ACTIONS      (1u << 6)
#define FW_PH_NORMALIZED_AVAILABILITY (1u << 7)

/* Runtime-only presentation state. It describes what can be shown now; it is
 * not a task scheduler protocol and need not be persisted in a document. */
typedef uint32_t fw_placeholder_phase;
#define FW_PLACEHOLDER_PHASE_NONE              0u
#define FW_PLACEHOLDER_PHASE_QUEUED            1u
#define FW_PLACEHOLDER_PHASE_RUNNING           2u
#define FW_PLACEHOLDER_PHASE_WAITING           3u
#define FW_PLACEHOLDER_PHASE_TRANSFERRING       4u
#define FW_PLACEHOLDER_PHASE_READY_FOR_HANDOFF  5u

typedef uint32_t fw_placeholder_progress_kind;
#define FW_PLACEHOLDER_PROGRESS_NONE          0u
#define FW_PLACEHOLDER_PROGRESS_INDETERMINATE 1u
#define FW_PLACEHOLDER_PROGRESS_FRACTION      2u

typedef struct fw_placeholder_progress_v1 {
    uint32_t struct_size;
    fw_placeholder_progress_kind kind;
    uint64_t completed;
    uint64_t total;
} fw_placeholder_progress_v1;

typedef struct fw_placeholder_style_v1 {
    uint32_t struct_size;
    fw_color_rgba_f32 background;
    fw_color_rgba_f32 border;
    fw_color_rgba_f32 icon;
    fw_color_rgba_f32 primary_text;
    fw_color_rgba_f32 secondary_text;
    fw_color_rgba_f32 action;
    float opacity;
    float border_width;
    float corner_radius;
    float content_padding;
    float gap;
    float icon_size;
    uint32_t flags;
} fw_placeholder_style_v1;

typedef struct fw_placeholder_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_string_view zone_id;
    fw_string_view content_kind;
    fw_string_view required_capability_id;
    fw_string_view accessible_label;
    fw_string_view diagnostic_code;
    fw_placeholder_reason reason;
    fw_placeholder_mode mode;
    fw_placeholder_action_mask permitted_actions;
    fw_optional_size_f32 resolved_size;
    fw_optional_size_f32 intrinsic_size;
    fw_optional_f32 intrinsic_aspect_ratio;
    fw_layout_constraints_v1 constraints;
    fw_placeholder_style_v1 style;
    fw_render_target_profile_v1 target;
    uint32_t fragment_index;
    uint32_t fragment_count;
    uint64_t presentation_revision;
    fw_placeholder_phase phase;
    fw_placeholder_progress_v1 progress;
    uint32_t stale;
    uint32_t flags;
} fw_placeholder_request_v1;

typedef uint32_t fw_placeholder_measure_source;
#define FW_PH_MEASURE_RESOLVED            1u
#define FW_PH_MEASURE_EXPLICIT_CONSTRAINT 2u
#define FW_PH_MEASURE_WIDTH_AND_RATIO     3u
#define FW_PH_MEASURE_HEIGHT_AND_RATIO    4u
#define FW_PH_MEASURE_INTRINSIC           5u
#define FW_PH_MEASURE_KIND_FALLBACK       6u
#define FW_PH_MEASURE_GENERIC_FALLBACK    7u

typedef struct fw_placeholder_measure_result_v1 {
    uint32_t struct_size;
    fw_size_f32 size;
    fw_placeholder_measure_source source;
    fw_placeholder_normalization_flags normalization_flags;
    uint32_t flags;
} fw_placeholder_measure_result_v1;

typedef uint32_t fw_placeholder_visual_density;
#define FW_PH_VISUAL_NONE    0u
#define FW_PH_VISUAL_OUTLINE 1u
#define FW_PH_VISUAL_ICON    2u
#define FW_PH_VISUAL_TITLE   3u
#define FW_PH_VISUAL_DETAIL  4u
#define FW_PH_VISUAL_ACTIONS 5u

typedef struct fw_placeholder_render_result_v1 {
    uint32_t struct_size;
    uint32_t emitted_command_count;
    fw_placeholder_visual_density visual_density;
    fw_placeholder_action_mask visible_actions;
    fw_placeholder_normalization_flags normalization_flags;
    uint64_t cache_key_high;
    uint64_t cache_key_low;
    uint32_t flags;
} fw_placeholder_render_result_v1;

typedef struct fw_placeholder_semantics_v1 {
    uint32_t struct_size;
    fw_semantics_role role;
    fw_string_view accessible_label;
    fw_string_view status_localization_key;
    fw_placeholder_action_mask available_actions;
    fw_rect_f32 bounds;
    uint32_t hidden_visually;
    uint32_t stale;
    fw_placeholder_phase phase;
    uint32_t flags;
} fw_placeholder_semantics_v1;

typedef struct fw_placeholder_hit_test_request_v1 {
    uint32_t struct_size;
    fw_placeholder_request_v1 placeholder;
    fw_rect_f32 bounds;
    fw_point_f32 point;
} fw_placeholder_hit_test_request_v1;

typedef struct fw_placeholder_hit_test_result_v1 {
    uint32_t struct_size;
    uint32_t hit;
    uint32_t action;
    uint32_t flags;
} fw_placeholder_hit_test_result_v1;

typedef struct fw_placeholder_services_v1 {
    uint32_t struct_size;
    const fw_display_list_sink_v1 *display_list;
    const fw_text_service_v1 *text;
    fw_string_view locale;
    uint32_t text_direction;
    uint64_t monotonic_time_ms;
    uint32_t flags;
} fw_placeholder_services_v1;

typedef struct fw_placeholder_validation_result_v1 {
    uint32_t struct_size;
    fw_status status;
    fw_placeholder_normalization_flags normalization_flags;
    fw_string_view diagnostic_key;
} fw_placeholder_validation_result_v1;

typedef struct fw_placeholder_renderer_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(
        fw_plugin_handle plugin,
        const fw_placeholder_request_v1 *request,
        fw_placeholder_validation_result_v1 *out_result);
    fw_status(FW_CALL *measure)(
        fw_plugin_handle plugin,
        const fw_placeholder_request_v1 *request,
        fw_placeholder_measure_result_v1 *out_result);
    fw_status(FW_CALL *render)(
        fw_plugin_handle plugin,
        const fw_placeholder_request_v1 *request,
        fw_rect_f32 bounds,
        const fw_placeholder_services_v1 *services,
        fw_placeholder_render_result_v1 *out_result);
    fw_status(FW_CALL *build_semantics)(
        fw_plugin_handle plugin,
        const fw_placeholder_request_v1 *request,
        fw_rect_f32 bounds,
        fw_placeholder_semantics_v1 *out_semantics);
    fw_status(FW_CALL *hit_test)(
        fw_plugin_handle plugin,
        const fw_placeholder_hit_test_request_v1 *request,
        fw_placeholder_hit_test_result_v1 *out_result);
    fw_status(FW_CALL *get_parameter_schema)(
        fw_plugin_handle plugin,
        fw_string_view *out_schema_json);
} fw_placeholder_renderer_api_v1;

#if defined(FACETWIRE_PLACEHOLDER_RENDERER_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_placeholder_renderer_plugin_query(
    fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
