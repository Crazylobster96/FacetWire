/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_TEXT_FRAGMENT_SERVICE_H
#define FACETWIRE_TEXT_FRAGMENT_SERVICE_H

#include <facetwire/display_list.h>
#include <facetwire/flow_layout.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_text_exclusion_rect_v1 {
    uint32_t struct_size;
    fw_rect_f32 rect;
    uint32_t flags;
} fw_text_exclusion_rect_v1;

typedef uint32_t fw_text_fragment_part_kind;
#define FW_TEXT_FRAGMENT_PART_TEXT 1u
#define FW_TEXT_FRAGMENT_PART_OBJECT 2u

#define FW_TEXT_FRAGMENT_SERVICE_INLINE_PARTS (1u << 0)

typedef struct fw_text_inline_object_v1 {
    uint32_t struct_size;
    fw_string_view item_id;
    size_t text_offset_utf8_byte;
    fw_size_f32 size;
    fw_edge_insets_f32 margins;
    float offset_x;
    float offset_y;
    fw_flow_baseline_mode baseline_mode;
    uint64_t layout_fingerprint_high;
    uint64_t layout_fingerprint_low;
    uint32_t flags;
} fw_text_inline_object_v1;

typedef struct fw_text_fragment_part_v1 {
    uint32_t struct_size;
    fw_text_fragment_part_kind kind;
    size_t inline_object_index;
    fw_rect_f32 bounds;
    size_t text_start_utf8_byte;
    size_t text_end_utf8_byte;
    uint32_t flags;
} fw_text_fragment_part_v1;

typedef struct fw_text_fragment_request_v1 {
    uint32_t struct_size;
    fw_string_view paragraph_id;
    const fw_flow_segment_v1 *segments;
    size_t segment_count;
    size_t start_utf8_byte;
    fw_text_style_v1 style;
    fw_text_direction direction;
    fw_rect_f32 region;
    const fw_text_exclusion_rect_v1 *exclusions;
    size_t exclusion_count;
    uint32_t max_lines;
    uint32_t flags;
    const fw_text_inline_object_v1 *inline_objects;
    size_t inline_object_count;
    size_t start_inline_object_index;
    fw_text_fragment_part_v1 *parts;
    size_t part_capacity;
} fw_text_fragment_request_v1;

typedef struct fw_text_fragment_metrics_v1 {
    uint32_t struct_size;
    size_t end_utf8_byte;
    fw_rect_f32 used_bounds;
    uint32_t line_count;
    uint32_t reached_end;
    uint32_t break_flags;
    uint64_t fingerprint_high;
    uint64_t fingerprint_low;
    uint32_t flags;
    size_t end_inline_object_index;
    size_t part_count;
} fw_text_fragment_metrics_v1;

typedef struct fw_text_fragment_service_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *measure_next)(void *user_data,
        const fw_text_fragment_request_v1 *request,
        fw_text_fragment_metrics_v1 *out_metrics);
    fw_status(FW_CALL *draw_exact)(void *user_data,
        const fw_text_fragment_request_v1 *request,
        const fw_text_fragment_metrics_v1 *expected,
        const fw_display_list_sink_v1 *display_list);
    uint32_t flags;
} fw_text_fragment_service_v1;

#ifdef __cplusplus
}
#endif

#endif
