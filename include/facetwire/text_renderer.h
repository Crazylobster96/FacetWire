/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_TEXT_RENDERER_H
#define FACETWIRE_TEXT_RENDERER_H

#include <facetwire/renderer.h>
#include <facetwire/semantics.h>
#include <facetwire/text_service_v2.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_TEXT_RENDERER_CAPABILITY_ID "facetwire.renderer.text"
#define FW_TEXT_RENDERER_INTERFACE_ID "facetwire.renderer.text.v1"
#define FW_TEXT_RENDERER_INTERFACE_VERSION 1u

typedef uint32_t fw_text_vertical_align;
#define FW_TEXT_ALIGN_TOP    0u
#define FW_TEXT_ALIGN_MIDDLE 1u
#define FW_TEXT_ALIGN_BOTTOM 2u

typedef uint32_t fw_text_overflow;
#define FW_TEXT_OVERFLOW_VISIBLE  0u
#define FW_TEXT_OVERFLOW_CLIP     1u
#define FW_TEXT_OVERFLOW_ELLIPSIS 2u
#define FW_TEXT_OVERFLOW_SCROLL   3u

typedef struct fw_edge_insets_f32 {
    float left;
    float top;
    float right;
    float bottom;
} fw_edge_insets_f32;

typedef struct fw_text_style_v1 {
    uint32_t struct_size;
    const fw_string_view *font_families;
    size_t font_family_count;
    fw_string_view font_resource_id;
    float font_size;
    uint32_t font_weight;
    fw_text_font_style font_style;
    float line_height_multiplier;
    float letter_spacing;
    fw_color_rgba_f32 color;
    fw_color_rgba_f32 background_color;
    fw_text_decoration_mask decorations;
    uint32_t has_background_color;
    uint32_t flags;
} fw_text_style_v1;

typedef struct fw_text_layout_policy_v1 {
    uint32_t struct_size;
    fw_text_horizontal_align horizontal_align;
    fw_text_vertical_align vertical_align;
    fw_text_wrap_mode wrap;
    fw_text_overflow overflow;
    uint32_t max_lines;
    fw_edge_insets_f32 padding;
    uint32_t flags;
} fw_text_layout_policy_v1;

typedef struct fw_text_session_v1 {
    uint32_t struct_size;
    uint64_t presentation_revision;
    float scroll_offset_y;
    uint32_t hidden_from_semantics;
    uint32_t flags;
} fw_text_session_v1;

typedef struct fw_text_renderer_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_string_view zone_id;
    fw_string_view text;
    fw_string_view language;
    fw_text_direction direction;
    uint32_t selectable;
    float opacity;
    fw_text_style_v1 style;
    fw_text_layout_policy_v1 layout;
    fw_layout_constraints_v1 constraints;
    fw_render_target_profile_v1 target;
    fw_text_session_v1 session;
    uint32_t flags;
} fw_text_renderer_request_v1;

typedef uint32_t fw_text_normalization_flags;
#define FW_TX_NORMALIZED_NONE          0u
#define FW_TX_NORMALIZED_NEWLINES      (1u << 0)
#define FW_TX_NORMALIZED_FONT_SCALE    (1u << 1)
#define FW_TX_NORMALIZED_CONSTRAINTS   (1u << 2)
#define FW_TX_NORMALIZED_SCROLL_OFFSET (1u << 3)
#define FW_TX_FONT_FALLBACK            (1u << 4)
#define FW_TX_VISUALLY_TRUNCATED       (1u << 5)

typedef struct fw_text_services_v1 {
    uint32_t struct_size;
    const fw_display_list_sink_v1 *display_list;
    const fw_text_service_v2 *text;
    uint32_t flags;
} fw_text_services_v1;

typedef struct fw_text_validation_result_v1 {
    uint32_t struct_size;
    fw_status status;
    fw_text_normalization_flags normalization_flags;
    fw_string_view diagnostic_key;
} fw_text_validation_result_v1;

typedef struct fw_text_measure_result_v1 {
    uint32_t struct_size;
    fw_size_f32 size;
    fw_size_f32 content_extent;
    fw_size_f32 viewport_extent;
    float first_baseline;
    uint32_t line_count;
    fw_text_normalization_flags normalization_flags;
    uint32_t flags;
} fw_text_measure_result_v1;

typedef struct fw_text_render_result_v1 {
    uint32_t struct_size;
    uint32_t emitted_command_count;
    fw_size_f32 content_extent;
    fw_size_f32 viewport_extent;
    float applied_scroll_offset_y;
    float max_scroll_offset_y;
    uint64_t cache_key_high;
    uint64_t cache_key_low;
    fw_text_normalization_flags normalization_flags;
    uint32_t flags;
} fw_text_render_result_v1;

typedef struct fw_text_semantics_v1 {
    uint32_t struct_size;
    uint32_t role;
    fw_string_view text;
    fw_string_view language;
    fw_text_direction direction;
    fw_rect_f32 bounds;
    uint32_t selectable;
    uint32_t scrollable;
    uint32_t visually_truncated;
    uint32_t hidden;
    float scroll_offset_y;
    float max_scroll_offset_y;
    uint32_t flags;
} fw_text_semantics_v1;

typedef struct fw_text_renderer_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_text_renderer_request_v1 *, fw_text_validation_result_v1 *);
    fw_status(FW_CALL *measure)(fw_plugin_handle,
        const fw_text_renderer_request_v1 *, const fw_text_services_v1 *,
        fw_text_measure_result_v1 *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_text_renderer_request_v1 *, fw_rect_f32,
        const fw_text_services_v1 *, fw_text_render_result_v1 *);
    fw_status(FW_CALL *build_semantics)(fw_plugin_handle,
        const fw_text_renderer_request_v1 *, fw_rect_f32,
        const fw_text_measure_result_v1 *, fw_text_semantics_v1 *);
    fw_status(FW_CALL *get_parameter_schema)(
        fw_plugin_handle, fw_string_view *out_schema_json);
} fw_text_renderer_api_v1;

#if defined(FACETWIRE_TEXT_RENDERER_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_text_renderer_plugin_query(fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
