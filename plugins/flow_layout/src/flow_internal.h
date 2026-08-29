/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_FLOW_INTERNAL_H
#define FACETWIRE_FLOW_INTERNAL_H

#include <facetwire/child_measure_service.h>
#include <facetwire/flow_layout.h>
#include <facetwire/text_fragment_service.h>

typedef struct fl_budget {
    uint32_t items;
    uint32_t segments;
    uint32_t pages;
    uint32_t fragments;
    uint32_t iterations;
} fl_budget;

typedef struct fl_hash {
    uint64_t high;
    uint64_t low;
} fl_hash;

fl_budget fl_resolve_budget(const fw_flow_budget_v1 *value);

void fl_hash_init(fl_hash *hash);
void fl_hash_bytes(fl_hash *hash, const void *data, size_t length);
void fl_hash_u64(fl_hash *hash, uint64_t value);
void fl_hash_f32(fl_hash *hash, float value);
void fl_hash_view(fl_hash *hash, fw_string_view value);
void fl_hash_insets(fl_hash *hash, fw_edge_insets_f32 value);
fl_hash fl_request_hash(const fw_flow_layout_request_v1 *request);

float fl_max(float left, float right);
float fl_min(float left, float right);
float fl_clamp_dimension(float value, float minimum, float maximum);

fw_status fl_emit(const fw_flow_plan_sink_v1 *sink,
    const fw_flow_fragment_v1 *fragment, fw_flow_layout_result_v1 *result,
    fl_hash *hash);
fw_status fl_make_fragment_id(char *buffer, size_t capacity,
    fw_string_view item_id, uint64_t revision, uint32_t ordinal,
    fw_string_view *out_view);

fw_status fl_compose_virtual_pages(
    const fw_flow_layout_request_v1 *request,
    const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink,
    fw_flow_layout_result_v1 *out_result);

#endif
