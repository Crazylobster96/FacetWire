/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_TEXT_SERVICE_V2_H
#define FACETWIRE_TEXT_SERVICE_V2_H

#include <facetwire/display_list.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t fw_text_direction;
#define FW_TEXT_DIRECTION_AUTO 0u
#define FW_TEXT_DIRECTION_LTR  1u
#define FW_TEXT_DIRECTION_RTL  2u

typedef uint32_t fw_text_horizontal_align;
#define FW_TEXT_ALIGN_START   0u
#define FW_TEXT_ALIGN_CENTER  1u
#define FW_TEXT_ALIGN_END     2u
#define FW_TEXT_ALIGN_JUSTIFY 3u

typedef uint32_t fw_text_font_style;
#define FW_TEXT_FONT_NORMAL  0u
#define FW_TEXT_FONT_ITALIC  1u
#define FW_TEXT_FONT_OBLIQUE 2u

typedef uint32_t fw_text_wrap_mode;
#define FW_TEXT_WRAP    0u
#define FW_TEXT_NO_WRAP 1u

typedef uint32_t fw_text_decoration_mask;
#define FW_TEXT_DECORATION_NONE         0u
#define FW_TEXT_DECORATION_UNDERLINE    (1u << 0)
#define FW_TEXT_DECORATION_LINE_THROUGH (1u << 1)

typedef struct fw_text_layout_request_v2 {
    uint32_t struct_size;
    fw_string_view text;
    fw_string_view language;
    const fw_string_view *font_families;
    size_t font_family_count;
    fw_string_view font_resource_id;
    float font_size;
    uint32_t font_weight;
    fw_text_font_style font_style;
    float line_height_multiplier;
    float letter_spacing;
    float max_width;
    float max_height;
    uint32_t max_lines;
    fw_text_direction direction;
    fw_text_horizontal_align horizontal_align;
    fw_text_wrap_mode wrap;
    uint32_t ellipsize;
    fw_text_decoration_mask decorations;
    uint32_t flags;
} fw_text_layout_request_v2;

typedef uint32_t fw_text_layout_flags_v2;
#define FW_TEXT_LAYOUT_FONT_FALLBACK (1u << 0)
#define FW_TEXT_LAYOUT_DID_TRUNCATE  (1u << 1)
#define FW_TEXT_LAYOUT_HAS_RTL       (1u << 2)
#define FW_TEXT_LAYOUT_RESOURCE_FONT (1u << 3)

typedef struct fw_text_layout_metrics_v2 {
    uint32_t struct_size;
    fw_size_f32 size;
    fw_rect_f32 ink_bounds;
    float first_baseline;
    float last_baseline;
    uint32_t line_count;
    fw_text_layout_flags_v2 flags;
    fw_string_view resolved_font_key;
} fw_text_layout_metrics_v2;

typedef struct fw_text_service_v2 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *layout_utf8_v2)(
        void *user_data,
        const fw_text_layout_request_v2 *request,
        fw_text_layout_handle *out_layout,
        fw_text_layout_metrics_v2 *out_metrics);
    void(FW_CALL *release_layout)(
        void *user_data,
        fw_text_layout_handle layout);
} fw_text_service_v2;

#ifdef __cplusplus
}
#endif

#endif
