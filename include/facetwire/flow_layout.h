/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_FLOW_LAYOUT_H
#define FACETWIRE_FLOW_LAYOUT_H

#include <facetwire/facetwire.h>
#include <facetwire/geometry.h>
#include <facetwire/render_target.h>
#include <facetwire/text_renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_FLOW_LAYOUT_CAPABILITY_ID "facetwire.layout.flow"
#define FW_FLOW_LAYOUT_INTERFACE_ID "facetwire.layout.flow.v1"
#define FW_FLOW_LAYOUT_INTERFACE_VERSION 1u
#define FW_FLOW_LAYOUT_CAPABILITY_KIND "facetwire.capability.layout"

typedef uint32_t fw_flow_item_kind;
#define FW_FLOW_ITEM_PARAGRAPH 1u
#define FW_FLOW_ITEM_OBJECT 2u

typedef uint32_t fw_flow_segment_kind;
#define FW_FLOW_SEGMENT_TEXT 1u
#define FW_FLOW_SEGMENT_OBJECT 2u

typedef uint32_t fw_flow_baseline_mode;
#define FW_FLOW_BASELINE_BASELINE 0u
#define FW_FLOW_BASELINE_MIDDLE 1u
#define FW_FLOW_BASELINE_TEXT_TOP 2u
#define FW_FLOW_BASELINE_TEXT_BOTTOM 3u

typedef uint32_t fw_flow_placement_mode;
#define FW_FLOW_PLACE_BLOCK 1u
#define FW_FLOW_PLACE_INLINE 2u
#define FW_FLOW_PLACE_FLOAT_START 3u
#define FW_FLOW_PLACE_FLOAT_END 4u
#define FW_FLOW_PLACE_OVERLAY 5u

typedef uint32_t fw_flow_page_mode;
#define FW_FLOW_CONTINUOUS 1u
#define FW_FLOW_VIRTUAL_PAGES 2u
#define FW_FLOW_COLUMNS 3u

typedef uint32_t fw_flow_fragment_kind;
#define FW_FLOW_FRAGMENT_TEXT 1u
#define FW_FLOW_FRAGMENT_OBJECT 2u
#define FW_FLOW_FRAGMENT_PLACEHOLDER 3u

#define FW_FLOW_FRAGMENT_FLAG_OVERLAY (1u << 0)
#define FW_FLOW_FRAGMENT_FLAG_BREAK_BEFORE (1u << 1)
#define FW_FLOW_FRAGMENT_FLAG_BREAK_AFTER_PREVIOUS (1u << 2)

#define FW_FLOW_DIAGNOSTIC_KEEP_TOGETHER_RELAXED (UINT64_C(1) << 0)
#define FW_FLOW_DIAGNOSTIC_KEEP_WITH_NEXT_RELAXED (UINT64_C(1) << 1)
#define FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED (UINT64_C(1) << 2)
#define FW_FLOW_DIAGNOSTIC_BACKTRACK_LIMIT (UINT64_C(1) << 3)

typedef struct fw_flow_break_policy_v1 {
    uint32_t struct_size;
    uint32_t break_before;
    uint32_t break_after;
    uint32_t keep_together;
    uint32_t keep_with_next;
    uint32_t orphans;
    uint32_t widows;
    uint32_t flags;
} fw_flow_break_policy_v1;

typedef struct fw_flow_placement_v1 {
    uint32_t struct_size;
    fw_flow_placement_mode mode;
    fw_edge_insets_f32 margins;
    float requested_width;
    float requested_height;
    float min_width;
    float min_height;
    float max_width;
    float max_height;
    float offset_x;
    float offset_y;
    int32_t z;
    uint32_t allow_scale_down;
    uint32_t allow_scale_up;
    uint32_t flags;
} fw_flow_placement_v1;

typedef struct fw_flow_segment_v1 {
    uint32_t struct_size;
    fw_flow_segment_kind kind;
    fw_string_view text;
    fw_string_view object_item_id;
    uint32_t baseline_mode;
    uint32_t flags;
} fw_flow_segment_v1;

typedef struct fw_flow_item_v1 {
    uint32_t struct_size;
    fw_string_view id;
    fw_flow_item_kind kind;
    const fw_flow_segment_v1 *segments;
    size_t segment_count;
    fw_string_view content_id;
    fw_string_view content_kind;
    fw_text_style_v1 text_style;
    fw_text_direction direction;
    fw_flow_placement_v1 placement;
    fw_flow_break_policy_v1 break_policy;
    uint32_t decorative;
    uint32_t flags;
    fw_string_view overlay_anchor_item_id;
    int32_t overlay_reading_order;
    uint32_t overlay_has_reading_order;
} fw_flow_item_v1;

typedef struct fw_flow_page_template_v1 {
    uint32_t struct_size;
    fw_flow_page_mode mode;
    fw_size_f32 page_size;
    fw_edge_insets_f32 margins;
    uint32_t column_count;
    float column_gap;
    float page_gap;
    float minimum_text_width;
    uint32_t flags;
} fw_flow_page_template_v1;

typedef struct fw_flow_budget_v1 {
    uint32_t struct_size;
    uint32_t max_items;
    uint32_t max_segments;
    uint32_t max_pages;
    uint32_t max_fragments;
    uint32_t max_active_floats;
    uint32_t max_backtrack_items;
    uint32_t max_iterations;
    uint32_t flags;
} fw_flow_budget_v1;

typedef struct fw_flow_layout_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_string_view flow_id;
    const fw_flow_item_v1 *items;
    size_t item_count;
    fw_flow_page_template_v1 page_template;
    fw_flow_budget_v1 budget;
    fw_render_target_profile_v1 target;
    uint64_t document_revision;
    uint64_t layout_revision;
    fw_string_view profile_key;
    uint32_t flags;
} fw_flow_layout_request_v1;

typedef struct fw_flow_page_v1 {
    uint32_t struct_size;
    uint32_t page_index;
    fw_string_view derived_page_id;
    fw_size_f32 size;
    fw_rect_f32 content_bounds;
    uint32_t column_count;
    uint32_t flags;
} fw_flow_page_v1;

typedef struct fw_flow_fragment_v1 {
    uint32_t struct_size;
    fw_flow_fragment_kind kind;
    fw_string_view derived_fragment_id;
    fw_string_view source_item_id;
    fw_string_view content_kind;
    uint32_t page_index;
    uint32_t column_index;
    fw_rect_f32 bounds;
    fw_rect_f32 clip;
    int32_t z;
    size_t text_start_utf8_byte;
    size_t text_end_utf8_byte;
    uint32_t continuation_before;
    uint32_t continuation_after;
    uint64_t layout_fingerprint_high;
    uint64_t layout_fingerprint_low;
    uint32_t flags;
} fw_flow_fragment_v1;

typedef struct fw_flow_plan_sink_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *begin_page)(void *, const fw_flow_page_v1 *);
    fw_status(FW_CALL *emit_fragment)(void *, const fw_flow_fragment_v1 *);
    fw_status(FW_CALL *end_page)(void *, uint32_t page_index);
} fw_flow_plan_sink_v1;

typedef struct fw_flow_validation_result_v1 {
    uint32_t struct_size;
    fw_status status;
    uint64_t diagnostic_flags;
    fw_string_view diagnostic_key;
} fw_flow_validation_result_v1;

typedef struct fw_flow_layout_result_v1 {
    uint32_t struct_size;
    uint32_t page_count;
    uint32_t fragment_count;
    uint32_t text_fragment_count;
    uint32_t object_fragment_count;
    fw_size_f32 continuous_extent;
    uint64_t plan_key_high;
    uint64_t plan_key_low;
    uint64_t diagnostic_flags;
    uint32_t complete;
    uint32_t flags;
} fw_flow_layout_result_v1;

struct fw_text_fragment_service_v1;
struct fw_child_measure_service_v1;

typedef struct fw_flow_layout_services_v1 {
    uint32_t struct_size;
    const struct fw_text_fragment_service_v1 *text;
    const struct fw_child_measure_service_v1 *children;
    uint32_t flags;
} fw_flow_layout_services_v1;

typedef struct fw_flow_layout_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_flow_layout_request_v1 *, fw_flow_validation_result_v1 *);
    fw_status(FW_CALL *compose)(fw_plugin_handle,
        const fw_flow_layout_request_v1 *, const fw_flow_layout_services_v1 *,
        const fw_flow_plan_sink_v1 *, fw_flow_layout_result_v1 *);
    fw_status(FW_CALL *get_parameter_schema)(
        fw_plugin_handle, fw_string_view *out_schema_json);
} fw_flow_layout_api_v1;

FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_flow_layout_plugin_query(fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
