/* SPDX-License-Identifier: MPL-2.0 */
#include "flow_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fl_virtual_context {
    const fw_flow_layout_request_v1 *request;
    const fw_flow_layout_services_v1 *services;
    const fw_flow_plan_sink_v1 *sink;
    fw_flow_layout_result_v1 *result;
    fl_budget budget;
    fl_hash page_seed;
    fl_hash plan_hash;
    fw_rect_f32 content;
    fw_rect_f32 region;
    uint32_t page_index;
    uint32_t column_index;
    uint32_t column_count;
    uint32_t ordinal;
    uint32_t iterations;
    uint32_t page_open;
    uint32_t page_has_fragments;
    uint32_t column_has_fragments;
    uint32_t continuous;
    float cursor_y;
    float previous_bottom;
    float column_gap;
    float max_layout_bottom;
    fw_text_exclusion_rect_v1 *active_floats;
    size_t active_float_count;
    size_t active_float_capacity;
    struct fl_anchor_record *anchors;
    size_t anchor_count;
    uint32_t pending_fragment_flags;
} fl_virtual_context;

typedef struct fl_anchor_record {
    fw_string_view item_id;
    fw_rect_f32 bounds;
    fw_rect_f32 clip;
    uint32_t page_index;
    uint32_t column_index;
    uint32_t valid;
} fl_anchor_record;

typedef struct fl_inline_child {
    const fw_flow_item_v1 *item;
    fw_child_measure_result_v1 measured;
} fl_inline_child;

static int fl_vp_bool(uint32_t value) {
    return value <= 1u;
}

static int fl_vp_finite(float value) {
    return isfinite(value);
}

static int fl_vp_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
}

static uint32_t fl_vp_line_rule(uint32_t value) {
    return value == 0u ? 2u : value;
}

static int fl_vp_view_equal(fw_string_view left, fw_string_view right) {
    return left.length == right.length &&
        (left.length == 0u ||
            memcmp(left.data, right.data, left.length) == 0);
}

static int fl_vp_text_service_base_valid(
    const fw_text_fragment_service_v1 *text) {
    return text != NULL &&
        text->struct_size >= offsetof(fw_text_fragment_service_v1, flags) &&
        text->measure_next != NULL;
}

static int fl_vp_text_service_inline_valid(
    const fw_text_fragment_service_v1 *text) {
    return fl_vp_text_service_base_valid(text) &&
        text->struct_size >= sizeof(*text) &&
        (text->flags & FW_TEXT_FRAGMENT_SERVICE_INLINE_PARTS) != 0u;
}

static size_t fl_vp_paragraph_bytes(const fw_flow_item_v1 *item) {
    size_t total = 0u;
    size_t index;
    for (index = 0u; index < item->segment_count; ++index) {
        if (item->segments[index].kind == FW_FLOW_SEGMENT_TEXT)
            total += item->segments[index].text.length;
    }
    return total;
}

static int fl_vp_offset_boundary(const fw_flow_item_v1 *item, size_t offset) {
    size_t cursor = 0u;
    size_t index;
    for (index = 0u; index < item->segment_count; ++index) {
        const fw_string_view text = item->segments[index].text;
        if (offset == cursor) return 1;
        if (offset < cursor + text.length) {
            const unsigned char value =
                (unsigned char)text.data[offset - cursor];
            return (value & 0xc0u) != 0x80u;
        }
        cursor += text.length;
    }
    return offset == cursor;
}

static void fl_vp_reset_column(fl_virtual_context *context) {
    const float gaps = (float)(context->column_count - 1u) *
        context->column_gap;
    const float width = (context->content.width - gaps) /
        (float)context->column_count;
    context->region.x = context->content.x +
        (float)context->column_index * (width + context->column_gap);
    context->region.y = context->content.y;
    context->region.width = width;
    context->region.height = context->content.height;
    context->cursor_y = context->region.y;
    context->previous_bottom = 0.0f;
    context->column_has_fragments = 0u;
    context->active_float_count = 0u;
}

static float fl_vp_rect_bottom(const fw_rect_f32 *rect) {
    return rect->y + rect->height;
}

static int fl_vp_vertical_overlap(const fw_rect_f32 *rect, float top,
    float bottom) {
    return rect->y < bottom - 0.0001f &&
        fl_vp_rect_bottom(rect) > top + 0.0001f;
}

static void fl_vp_expire_floats(fl_virtual_context *context) {
    size_t read_index;
    size_t write_index = 0u;
    for (read_index = 0u; read_index < context->active_float_count;
         ++read_index) {
        fw_text_exclusion_rect_v1 value =
            context->active_floats[read_index];
        if (fl_vp_rect_bottom(&value.rect) <=
            context->cursor_y + 0.0001f) continue;
        context->active_floats[write_index++] = value;
    }
    context->active_float_count = write_index;
}

static void fl_vp_available_span(const fl_virtual_context *context,
    float top, float bottom, float *out_left, float *out_right,
    float *out_next_bottom) {
    const float center = context->region.x + context->region.width * 0.5f;
    float left = context->region.x;
    float right = context->region.x + context->region.width;
    float next_bottom = context->region.y + context->region.height;
    size_t index;
    for (index = 0u; index < context->active_float_count; ++index) {
        const fw_rect_f32 *rect = &context->active_floats[index].rect;
        if (!fl_vp_vertical_overlap(rect, top, bottom)) continue;
        if (rect->x + rect->width * 0.5f <= center)
            left = fl_max(left, rect->x + rect->width);
        else
            right = fl_min(right, rect->x);
        next_bottom = fl_min(next_bottom, fl_vp_rect_bottom(rect));
    }
    *out_left = left;
    *out_right = right;
    *out_next_bottom = next_bottom;
}

static fw_status fl_vp_make_text_space(fl_virtual_context *context,
    float line_height) {
    const float minimum = context->request->page_template.minimum_text_width;
    const float region_bottom = context->region.y + context->region.height;
    for (;;) {
        float left;
        float right;
        float next_bottom;
        fl_vp_expire_floats(context);
        if (context->active_float_count == 0u || minimum == 0.0f)
            return FW_STATUS_OK;
        fl_vp_available_span(context, context->cursor_y,
            context->cursor_y + line_height, &left, &right, &next_bottom);
        if (right - left + 0.0001f >= minimum)
            return FW_STATUS_OK;
        if (next_bottom <= context->cursor_y + 0.0001f ||
            next_bottom > region_bottom + 0.0001f)
            return FW_STATUS_INVALID_STATE;
        context->cursor_y = next_bottom;
        context->previous_bottom = 0.0f;
    }
}

static fw_status fl_vp_open_page(fl_virtual_context *context) {
    fw_flow_page_v1 page;
    char page_id[96];
    int length;
    fw_status status;
    if (context->result->page_count >= context->budget.pages)
        return FW_STATUS_RESOURCE_LIMIT;
    length = snprintf(page_id, sizeof(page_id),
        "vp:%016llx%016llx:%llu:%u",
        (unsigned long long)context->page_seed.high,
        (unsigned long long)context->page_seed.low,
        (unsigned long long)context->request->layout_revision,
        context->page_index);
    if (length < 0 || (size_t)length >= sizeof(page_id))
        return FW_STATUS_RESOURCE_LIMIT;
    memset(&page, 0, sizeof(page));
    page.struct_size = sizeof(page);
    page.page_index = context->page_index;
    page.derived_page_id.data = page_id;
    page.derived_page_id.length = (size_t)length;
    page.size = context->request->page_template.page_size;
    page.content_bounds = context->content;
    page.column_count = context->column_count;
    status = context->sink->begin_page(context->sink->user_data, &page);
    if (status != FW_STATUS_OK) return FW_STATUS_SINK_REJECTED;
    context->page_open = 1u;
    context->page_has_fragments = 0u;
    context->column_index = 0u;
    fl_vp_reset_column(context);
    ++context->result->page_count;
    fl_hash_u64(&context->plan_hash, context->page_index);
    return FW_STATUS_OK;
}

static fw_status fl_vp_close_page(fl_virtual_context *context) {
    fw_status status;
    if (context->page_open == 0u) return FW_STATUS_OK;
    status = context->sink->end_page(context->sink->user_data,
        context->page_index);
    context->page_open = 0u;
    return status == FW_STATUS_OK ? FW_STATUS_OK : FW_STATUS_SINK_REJECTED;
}

static fw_status fl_vp_next_page(fl_virtual_context *context) {
    fw_status status = fl_vp_close_page(context);
    if (status != FW_STATUS_OK) return status;
    ++context->page_index;
    return fl_vp_open_page(context);
}

static fw_status fl_vp_next_region(fl_virtual_context *context) {
    if (context->column_index + 1u < context->column_count) {
        ++context->column_index;
        fl_vp_reset_column(context);
        return FW_STATUS_OK;
    }
    if (context->continuous != 0u) return FW_STATUS_RESOURCE_LIMIT;
    return fl_vp_next_page(context);
}

static fl_anchor_record *fl_vp_find_anchor(fl_virtual_context *context,
    fw_string_view item_id) {
    size_t index;
    for (index = 0u; index < context->anchor_count; ++index) {
        if (fl_vp_view_equal(context->anchors[index].item_id, item_id))
            return &context->anchors[index];
    }
    return NULL;
}

static fw_status fl_vp_emit(fl_virtual_context *context,
    const fw_flow_fragment_v1 *fragment) {
    fw_flow_fragment_v1 emitted = *fragment;
    fl_anchor_record *anchor;
    fw_status status;
    emitted.flags |= context->pending_fragment_flags;
    context->pending_fragment_flags = 0u;
    status = fl_emit(context->sink, &emitted,
        context->result, &context->plan_hash);
    if (status == FW_STATUS_OK) {
        context->page_has_fragments = 1u;
        context->column_has_fragments = 1u;
        anchor = fl_vp_find_anchor(context, emitted.source_item_id);
        if (anchor != NULL && anchor->valid == 0u) {
            anchor->bounds = emitted.bounds;
            anchor->clip = emitted.clip;
            anchor->page_index = emitted.page_index;
            anchor->column_index = emitted.column_index;
            anchor->valid = 1u;
        }
    }
    return status;
}

static fw_status fl_vp_apply_explicit_break(fl_virtual_context *context,
    uint32_t fragment_flag) {
    context->pending_fragment_flags |= fragment_flag;
    if (context->continuous != 0u ||
        context->column_has_fragments == 0u)
        return FW_STATUS_OK;
    return fl_vp_next_region(context);
}

static int fl_vp_metrics_valid(const fw_flow_item_v1 *item,
    const fw_text_fragment_request_v1 *request, size_t total,
    const fw_text_fragment_metrics_v1 *metrics) {
    return metrics->struct_size >=
            offsetof(fw_text_fragment_metrics_v1,
                end_inline_object_index) &&
        fl_vp_bool(metrics->reached_end) &&
        fl_vp_finite(metrics->used_bounds.x) &&
        fl_vp_finite(metrics->used_bounds.y) &&
        fl_vp_nonnegative(metrics->used_bounds.width) &&
        fl_vp_nonnegative(metrics->used_bounds.height) &&
        metrics->used_bounds.x >= request->region.x &&
        metrics->used_bounds.y >= request->region.y &&
        metrics->used_bounds.x + metrics->used_bounds.width <=
            request->region.x + request->region.width &&
        metrics->used_bounds.y + metrics->used_bounds.height <=
            request->region.y + request->region.height &&
        metrics->end_utf8_byte >= request->start_utf8_byte &&
        metrics->end_utf8_byte <= total &&
        fl_vp_offset_boundary(item, metrics->end_utf8_byte) &&
        ((metrics->reached_end != 0u) ==
            (metrics->end_utf8_byte == total)) &&
        (metrics->reached_end != 0u ||
            metrics->end_utf8_byte != request->start_utf8_byte);
}

static int fl_vp_has_inline_segments(const fw_flow_item_v1 *item) {
    size_t index;
    for (index = 0u; index < item->segment_count; ++index) {
        if (item->segments[index].kind == FW_FLOW_SEGMENT_OBJECT) return 1;
    }
    return 0;
}

static fw_rect_f32 fl_vp_following_region(
    const fl_virtual_context *context) {
    fw_rect_f32 result = context->region;
    uint32_t column = context->column_index + 1u;
    if (column >= context->column_count) column = 0u;
    result.x = context->content.x +
        (float)column * (context->region.width + context->column_gap);
    result.y = context->content.y;
    result.height = context->content.height;
    return result;
}

static fw_status fl_vp_balance_widows(fl_virtual_context *context,
    const fw_flow_item_v1 *item, fw_text_fragment_request_v1 *request,
    fw_text_fragment_metrics_v1 *metrics, size_t total) {
    const fw_text_fragment_service_v1 *text = context->services->text;
    const uint32_t orphans =
        fl_vp_line_rule(item->break_policy.orphans);
    const uint32_t widows =
        fl_vp_line_rule(item->break_policy.widows);
    fw_text_fragment_request_v1 probe_request;
    fw_text_fragment_metrics_v1 probe_metrics;
    fw_status status;
    uint32_t missing;
    uint32_t reduced_lines;
    size_t original_end;
    if (context->continuous != 0u || metrics->reached_end != 0u ||
        widows <= 1u || request->inline_object_count != 0u)
        return FW_STATUS_OK;
    if (++context->iterations > context->budget.iterations)
        return FW_STATUS_RESOURCE_LIMIT;
    probe_request = *request;
    probe_request.start_utf8_byte = metrics->end_utf8_byte;
    probe_request.region = fl_vp_following_region(context);
    probe_request.exclusions = NULL;
    probe_request.exclusion_count = 0u;
    probe_request.max_lines = 0u;
    memset(&probe_metrics, 0, sizeof(probe_metrics));
    probe_metrics.struct_size = sizeof(probe_metrics);
    status = text->measure_next(text->user_data, &probe_request,
        &probe_metrics);
    if (status != FW_STATUS_OK) {
        context->result->diagnostic_flags |=
            FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED;
        return FW_STATUS_OK;
    }
    if (!fl_vp_metrics_valid(item, &probe_request, total, &probe_metrics))
        return FW_STATUS_INVALID_STATE;
    if (probe_metrics.reached_end == 0u ||
        probe_metrics.line_count >= widows)
        return FW_STATUS_OK;
    missing = widows - probe_metrics.line_count;
    if (metrics->line_count <= missing ||
        metrics->line_count - missing < orphans) {
        context->result->diagnostic_flags |=
            FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED;
        return FW_STATUS_OK;
    }
    reduced_lines = metrics->line_count - missing;
    original_end = metrics->end_utf8_byte;
    request->max_lines = reduced_lines;
    memset(metrics, 0, sizeof(*metrics));
    metrics->struct_size = sizeof(*metrics);
    if (++context->iterations > context->budget.iterations)
        return FW_STATUS_RESOURCE_LIMIT;
    status = text->measure_next(text->user_data, request, metrics);
    if (status != FW_STATUS_OK) return status;
    if (!fl_vp_metrics_valid(item, request, total, metrics))
        return FW_STATUS_INVALID_STATE;
    if (metrics->end_utf8_byte >= original_end ||
        metrics->line_count > reduced_lines) {
        context->result->diagnostic_flags |=
            FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED;
    }
    return FW_STATUS_OK;
}

static fw_status fl_vp_compose_inline_paragraph(
    fl_virtual_context *context, const fw_flow_item_v1 *item);

static fw_status fl_vp_compose_paragraph(fl_virtual_context *context,
    const fw_flow_item_v1 *item) {
    const fw_text_fragment_service_v1 *text = context->services->text;
    const size_t total = fl_vp_paragraph_bytes(item);
    size_t start = 0u;
    uint32_t continuation = 0u;
    uint32_t initial_region_retry = 0u;
    if (fl_vp_has_inline_segments(item))
        return fl_vp_compose_inline_paragraph(context, item);
    if (!fl_vp_text_service_base_valid(text))
        return FW_STATUS_INVALID_ARGUMENT;
    for (;;) {
        fw_text_fragment_request_v1 request;
        fw_text_fragment_metrics_v1 metrics;
        fw_flow_fragment_v1 fragment;
        char id[256];
        fw_status status;
        float remaining;
        const float line_height = item->text_style.font_size *
            item->text_style.line_height_multiplier;
        if (++context->iterations > context->budget.iterations ||
            context->result->fragment_count >= context->budget.fragments)
            return FW_STATUS_RESOURCE_LIMIT;
        status = fl_vp_make_text_space(context, line_height);
        if (status != FW_STATUS_OK) return status;
        remaining = context->region.y + context->region.height -
            context->cursor_y;
        if (remaining + 0.0001f < line_height) {
            if (context->column_has_fragments == 0u)
                return FW_STATUS_RESOURCE_LIMIT;
            status = fl_vp_next_region(context);
            if (status != FW_STATUS_OK) return status;
            if (continuation == 0u)
                context->cursor_y += item->placement.margins.top;
            remaining = context->region.y + context->region.height -
                context->cursor_y;
        }
        memset(&request, 0, sizeof(request));
        request.struct_size = sizeof(request);
        request.paragraph_id = item->id;
        request.segments = item->segments;
        request.segment_count = item->segment_count;
        request.start_utf8_byte = start;
        request.style = item->text_style;
        request.direction = item->direction;
        request.region.x = context->region.x;
        request.region.y = context->cursor_y;
        request.region.width = context->region.width;
        request.region.height = remaining;
        request.exclusions = context->active_floats;
        request.exclusion_count = context->active_float_count;
        memset(&metrics, 0, sizeof(metrics));
        metrics.struct_size = sizeof(metrics);
        status = text->measure_next(text->user_data, &request, &metrics);
        if (status == FW_STATUS_RESOURCE_LIMIT &&
            context->column_has_fragments != 0u &&
            request.region.height + 0.0001f < context->region.height) {
            status = fl_vp_next_region(context);
            if (status != FW_STATUS_OK) return status;
            if (continuation == 0u)
                context->cursor_y += item->placement.margins.top;
            continue;
        }
        if (status != FW_STATUS_OK) return status;
        if (!fl_vp_metrics_valid(item, &request, total, &metrics))
            return FW_STATUS_INVALID_STATE;
        if (start == 0u && metrics.reached_end == 0u &&
            (item->break_policy.keep_together != 0u ||
             metrics.line_count <
                fl_vp_line_rule(item->break_policy.orphans))) {
            if (context->column_has_fragments != 0u &&
                initial_region_retry == 0u) {
                status = fl_vp_next_region(context);
                if (status != FW_STATUS_OK) return status;
                context->cursor_y += item->placement.margins.top;
                initial_region_retry = 1u;
                continue;
            }
            context->result->diagnostic_flags |=
                item->break_policy.keep_together != 0u ?
                    FW_FLOW_DIAGNOSTIC_KEEP_TOGETHER_RELAXED :
                    FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED;
        }
        status = fl_vp_balance_widows(context, item, &request, &metrics,
            total);
        if (status != FW_STATUS_OK) return status;
        memset(&fragment, 0, sizeof(fragment));
        fragment.struct_size = sizeof(fragment);
        fragment.kind = FW_FLOW_FRAGMENT_TEXT;
        status = fl_make_fragment_id(id, sizeof(id), item->id,
            context->request->layout_revision, context->ordinal++,
            &fragment.derived_fragment_id);
        if (status != FW_STATUS_OK) return status;
        fragment.source_item_id = item->id;
        fragment.page_index = context->page_index;
        fragment.column_index = context->column_index;
        fragment.bounds = metrics.used_bounds;
        fragment.clip = context->region;
        fragment.z = item->placement.z;
        fragment.text_start_utf8_byte = start;
        fragment.text_end_utf8_byte = metrics.end_utf8_byte;
        fragment.continuation_before = continuation;
        fragment.continuation_after = metrics.reached_end == 0u;
        fragment.layout_fingerprint_high = metrics.fingerprint_high;
        fragment.layout_fingerprint_low = metrics.fingerprint_low;
        status = fl_vp_emit(context, &fragment);
        if (status != FW_STATUS_OK) return status;
        context->cursor_y = fl_max(context->cursor_y,
            metrics.used_bounds.y + metrics.used_bounds.height);
        start = metrics.end_utf8_byte;
        if (metrics.reached_end != 0u) return FW_STATUS_OK;
        continuation = 1u;
        status = fl_vp_next_region(context);
        if (status != FW_STATUS_OK) return status;
    }
}

static fw_status fl_vp_measure_object(fl_virtual_context *context,
    const fw_flow_item_v1 *item, const fw_text_style_v1 *paragraph_style,
    fw_child_measure_result_v1 *measured, fw_size_f32 *size) {
    const fw_child_measure_service_v1 *children =
        context->services->children;
    fw_child_measure_request_v1 request;
    fw_status status;
    float ratio = 0.0f;
    if (children == NULL || children->struct_size < sizeof(*children) ||
        children->measure_child == NULL) return FW_STATUS_INVALID_ARGUMENT;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.item_id = item->id;
    request.content_id = item->content_id;
    request.content_kind = item->content_kind;
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.min_width = item->placement.min_width;
    request.constraints.max_width = item->placement.max_width == 0.0f ?
        context->region.width :
        fl_min(item->placement.max_width, context->region.width);
    request.constraints.min_height = item->placement.min_height;
    request.constraints.max_height = item->placement.max_height == 0.0f ?
        context->region.height :
        fl_min(item->placement.max_height, context->region.height);
    request.constraints.em_size = paragraph_style->font_size;
    request.constraints.line_height = paragraph_style->font_size *
        paragraph_style->line_height_multiplier;
    request.target = context->request->target;
    memset(measured, 0, sizeof(*measured));
    measured->struct_size = sizeof(*measured);
    status = children->measure_child(children->user_data, &request, measured);
    if (status != FW_STATUS_OK) return status;
    if (measured->struct_size < sizeof(*measured) ||
        !fl_vp_bool(measured->has_intrinsic_size) ||
        !fl_vp_bool(measured->splittable) ||
        !fl_vp_bool(measured->used_fallback) ||
        !fl_vp_nonnegative(measured->intrinsic_size.width) ||
        !fl_vp_nonnegative(measured->intrinsic_size.height) ||
        !fl_vp_nonnegative(measured->fallback_size.width) ||
        !fl_vp_nonnegative(measured->fallback_size.height) ||
        !fl_vp_bool(measured->aspect_ratio.has_value) ||
        (measured->aspect_ratio.has_value != 0u &&
            (!fl_vp_finite(measured->aspect_ratio.value) ||
             measured->aspect_ratio.value <= 0.0f)))
        return FW_STATUS_INVALID_STATE;
    *size = measured->has_intrinsic_size != 0u ?
        measured->intrinsic_size : measured->fallback_size;
    if (measured->aspect_ratio.has_value != 0u)
        ratio = measured->aspect_ratio.value;
    else if (size->height > 0.0f)
        ratio = size->width / size->height;
    if (item->placement.requested_width > 0.0f)
        size->width = item->placement.requested_width;
    if (item->placement.requested_height > 0.0f)
        size->height = item->placement.requested_height;
    if (item->placement.requested_width > 0.0f &&
        item->placement.requested_height == 0.0f && ratio > 0.0f)
        size->height = size->width / ratio;
    if (item->placement.requested_height > 0.0f &&
        item->placement.requested_width == 0.0f && ratio > 0.0f)
        size->width = size->height * ratio;
    size->width = fl_clamp_dimension(size->width,
        item->placement.min_width, item->placement.max_width);
    size->height = fl_clamp_dimension(size->height,
        item->placement.min_height, item->placement.max_height);
    return FW_STATUS_OK;
}

static fw_status fl_vp_fit_object(const fw_flow_item_v1 *item,
    fw_size_f32 *size, float available_width, float available_height) {
    if (size->width > available_width || size->height > available_height) {
        float scale;
        if (item->placement.allow_scale_down == 0u ||
            size->width <= 0.0f || size->height <= 0.0f)
            return FW_STATUS_RESOURCE_LIMIT;
        scale = fl_min(available_width / size->width,
            available_height / size->height);
        if (!fl_vp_finite(scale) || scale <= 0.0f)
            return FW_STATUS_RESOURCE_LIMIT;
        if (scale < 1.0f) {
            size->width *= scale;
            size->height *= scale;
        }
    }
    if (size->width <= 0.0f || size->height <= 0.0f ||
        size->width < item->placement.min_width ||
        size->height < item->placement.min_height ||
        size->width > available_width + 0.0001f ||
        size->height > available_height + 0.0001f)
        return FW_STATUS_RESOURCE_LIMIT;
    return FW_STATUS_OK;
}

static const fw_flow_item_v1 *fl_vp_find_item(
    const fw_flow_layout_request_v1 *request, fw_string_view id) {
    size_t index;
    for (index = 0u; index < request->item_count; ++index) {
        const fw_flow_item_v1 *item = &request->items[index];
        if (item->id.length == id.length &&
            (id.length == 0u ||
             memcmp(item->id.data, id.data, id.length) == 0))
            return item;
    }
    return NULL;
}

static int fl_vp_part_bounds_valid(const fw_rect_f32 *bounds,
    const fw_rect_f32 *region) {
    return fl_vp_finite(bounds->x) && fl_vp_finite(bounds->y) &&
        fl_vp_nonnegative(bounds->width) &&
        fl_vp_nonnegative(bounds->height) && bounds->x >= region->x &&
        bounds->y >= region->y &&
        bounds->x + bounds->width <= region->x + region->width + 0.0001f &&
        bounds->y + bounds->height <= region->y + region->height + 0.0001f;
}

static int fl_vp_inline_metrics_valid(const fw_flow_item_v1 *item,
    const fw_text_fragment_request_v1 *request,
    const fw_text_fragment_metrics_v1 *metrics, size_t total) {
    size_t text_cursor = request->start_utf8_byte;
    size_t object_cursor = request->start_inline_object_index;
    size_t part_index;
    if (metrics->struct_size < sizeof(*metrics) ||
        !fl_vp_bool(metrics->reached_end) ||
        metrics->end_utf8_byte < request->start_utf8_byte ||
        metrics->end_utf8_byte > total ||
        !fl_vp_offset_boundary(item, metrics->end_utf8_byte) ||
        metrics->end_inline_object_index <
            request->start_inline_object_index ||
        metrics->end_inline_object_index > request->inline_object_count ||
        metrics->part_count > request->part_capacity ||
        !fl_vp_part_bounds_valid(&metrics->used_bounds, &request->region) ||
        ((metrics->reached_end != 0u) !=
            (metrics->end_utf8_byte == total &&
             metrics->end_inline_object_index ==
                request->inline_object_count)) ||
        (metrics->reached_end == 0u &&
         metrics->end_utf8_byte == request->start_utf8_byte &&
         metrics->end_inline_object_index ==
            request->start_inline_object_index))
        return 0;
    for (part_index = 0u; part_index < metrics->part_count; ++part_index) {
        const fw_text_fragment_part_v1 *part = &request->parts[part_index];
        if (part->struct_size < sizeof(*part) ||
            !fl_vp_part_bounds_valid(&part->bounds, &request->region))
            return 0;
        if (part->kind == FW_TEXT_FRAGMENT_PART_TEXT) {
            size_t limit = metrics->end_utf8_byte;
            if (object_cursor < request->inline_object_count)
                limit = request->inline_objects[object_cursor].
                    text_offset_utf8_byte;
            if (part->text_start_utf8_byte != text_cursor ||
                part->text_end_utf8_byte <= text_cursor ||
                part->text_end_utf8_byte > limit ||
                !fl_vp_offset_boundary(item, part->text_end_utf8_byte))
                return 0;
            text_cursor = part->text_end_utf8_byte;
        } else if (part->kind == FW_TEXT_FRAGMENT_PART_OBJECT) {
            const fw_text_inline_object_v1 *object;
            if (part->inline_object_index != object_cursor ||
                object_cursor >= metrics->end_inline_object_index)
                return 0;
            object = &request->inline_objects[object_cursor];
            if (object->text_offset_utf8_byte != text_cursor ||
                fabsf(part->bounds.width - object->size.width) > 0.0001f ||
                fabsf(part->bounds.height - object->size.height) > 0.0001f)
                return 0;
            ++object_cursor;
        } else {
            return 0;
        }
    }
    return text_cursor == metrics->end_utf8_byte &&
        object_cursor == metrics->end_inline_object_index;
}

static fw_status fl_vp_emit_inline_parts(fl_virtual_context *context,
    const fw_flow_item_v1 *paragraph, const fl_inline_child *children,
    const fw_text_fragment_request_v1 *request,
    const fw_text_fragment_metrics_v1 *metrics) {
    size_t index;
    for (index = 0u; index < metrics->part_count; ++index) {
        const fw_text_fragment_part_v1 *part = &request->parts[index];
        const fw_flow_item_v1 *source = paragraph;
        fw_flow_fragment_v1 fragment;
        char id[256];
        fw_status status;
        if (context->result->fragment_count >= context->budget.fragments)
            return FW_STATUS_RESOURCE_LIMIT;
        memset(&fragment, 0, sizeof(fragment));
        fragment.struct_size = sizeof(fragment);
        if (part->kind == FW_TEXT_FRAGMENT_PART_OBJECT) {
            const fl_inline_child *child =
                &children[part->inline_object_index];
            source = child->item;
            fragment.kind = child->measured.used_fallback != 0u ?
                FW_FLOW_FRAGMENT_PLACEHOLDER : FW_FLOW_FRAGMENT_OBJECT;
            fragment.content_kind = source->content_kind;
            fragment.layout_fingerprint_high =
                child->measured.fingerprint_high;
            fragment.layout_fingerprint_low = child->measured.fingerprint_low;
        } else {
            fragment.kind = FW_FLOW_FRAGMENT_TEXT;
            fragment.text_start_utf8_byte = part->text_start_utf8_byte;
            fragment.text_end_utf8_byte = part->text_end_utf8_byte;
            fragment.continuation_before =
                request->start_utf8_byte != 0u ||
                request->start_inline_object_index != 0u || index != 0u;
            fragment.continuation_after =
                metrics->reached_end == 0u || index + 1u < metrics->part_count;
            fragment.layout_fingerprint_high = metrics->fingerprint_high;
            fragment.layout_fingerprint_low = metrics->fingerprint_low;
        }
        status = fl_make_fragment_id(id, sizeof(id), source->id,
            context->request->layout_revision, context->ordinal++,
            &fragment.derived_fragment_id);
        if (status != FW_STATUS_OK) return status;
        fragment.source_item_id = source->id;
        fragment.page_index = context->page_index;
        fragment.column_index = context->column_index;
        fragment.bounds = part->bounds;
        fragment.clip = context->region;
        fragment.z = source->placement.z;
        status = fl_vp_emit(context, &fragment);
        if (status != FW_STATUS_OK) return status;
    }
    return FW_STATUS_OK;
}

static fw_status fl_vp_compose_inline_paragraph(
    fl_virtual_context *context, const fw_flow_item_v1 *item) {
    const fw_text_fragment_service_v1 *text = context->services->text;
    const size_t total = fl_vp_paragraph_bytes(item);
    size_t inline_count = 0u;
    size_t segment_index;
    size_t text_offset = 0u;
    size_t start = 0u;
    size_t start_inline = 0u;
    uint32_t initial_region_retry = 0u;
    fl_inline_child *children = NULL;
    fw_text_inline_object_v1 *objects = NULL;
    fw_text_fragment_part_v1 *parts = NULL;
    fw_status status = FW_STATUS_OK;
    if (!fl_vp_text_service_inline_valid(text))
        return FW_STATUS_UNSUPPORTED;
    for (segment_index = 0u; segment_index < item->segment_count;
         ++segment_index) {
        if (item->segments[segment_index].kind == FW_FLOW_SEGMENT_OBJECT)
            ++inline_count;
    }
    if (inline_count == 0u) return FW_STATUS_INVALID_STATE;
    if (inline_count > SIZE_MAX / sizeof(*children) ||
        inline_count > SIZE_MAX / sizeof(*objects) ||
        item->segment_count > SIZE_MAX / sizeof(*parts))
        return FW_STATUS_RESOURCE_LIMIT;
    children = (fl_inline_child *)calloc(inline_count, sizeof(*children));
    objects = (fw_text_inline_object_v1 *)calloc(inline_count,
        sizeof(*objects));
    parts = (fw_text_fragment_part_v1 *)calloc(item->segment_count,
        sizeof(*parts));
    if (children == NULL || objects == NULL || parts == NULL) {
        status = FW_STATUS_OUT_OF_MEMORY;
        goto cleanup;
    }
    inline_count = 0u;
    for (segment_index = 0u; segment_index < item->segment_count;
         ++segment_index) {
        const fw_flow_segment_v1 *segment = &item->segments[segment_index];
        if (segment->kind == FW_FLOW_SEGMENT_TEXT) {
            text_offset += segment->text.length;
        } else {
            fl_inline_child *child = &children[inline_count];
            fw_text_inline_object_v1 *object = &objects[inline_count];
            float horizontal_margins;
            float vertical_margins;
            child->item = fl_vp_find_item(context->request,
                segment->object_item_id);
            if (child->item == NULL) {
                status = FW_STATUS_INVALID_STATE;
                goto cleanup;
            }
            if (++context->iterations > context->budget.iterations) {
                status = FW_STATUS_RESOURCE_LIMIT;
                goto cleanup;
            }
            status = fl_vp_measure_object(context, child->item,
                &item->text_style, &child->measured, &object->size);
            if (status != FW_STATUS_OK) goto cleanup;
            horizontal_margins = child->item->placement.margins.left +
                child->item->placement.margins.right;
            vertical_margins = child->item->placement.margins.top +
                child->item->placement.margins.bottom;
            if (horizontal_margins >= context->region.width ||
                vertical_margins >= context->region.height) {
                status = FW_STATUS_RESOURCE_LIMIT;
                goto cleanup;
            }
            status = fl_vp_fit_object(child->item, &object->size,
                context->region.width - horizontal_margins,
                context->region.height - vertical_margins);
            if (status != FW_STATUS_OK) goto cleanup;
            object->struct_size = sizeof(*object);
            object->item_id = child->item->id;
            object->text_offset_utf8_byte = text_offset;
            object->margins = child->item->placement.margins;
            object->offset_x = child->item->placement.offset_x;
            object->offset_y = child->item->placement.offset_y;
            object->baseline_mode = segment->baseline_mode;
            object->layout_fingerprint_high =
                child->measured.fingerprint_high;
            object->layout_fingerprint_low = child->measured.fingerprint_low;
            ++inline_count;
        }
    }
    while (status == FW_STATUS_OK) {
        fw_text_fragment_request_v1 request;
        fw_text_fragment_metrics_v1 metrics;
        float remaining;
        const float line_height = item->text_style.font_size *
            item->text_style.line_height_multiplier;
        size_t part_index;
        if (++context->iterations > context->budget.iterations) {
            status = FW_STATUS_RESOURCE_LIMIT;
            break;
        }
        status = fl_vp_make_text_space(context, line_height);
        if (status != FW_STATUS_OK) break;
        remaining = context->region.y + context->region.height -
            context->cursor_y;
        if (remaining + 0.0001f < line_height) {
            if (context->column_has_fragments == 0u) {
                status = FW_STATUS_RESOURCE_LIMIT;
                break;
            }
            status = fl_vp_next_region(context);
            if (status != FW_STATUS_OK) break;
            remaining = context->region.height;
        }
        memset(parts, 0, item->segment_count * sizeof(*parts));
        for (part_index = 0u; part_index < item->segment_count; ++part_index)
            parts[part_index].struct_size = sizeof(*parts);
        memset(&request, 0, sizeof(request));
        request.struct_size = sizeof(request);
        request.paragraph_id = item->id;
        request.segments = item->segments;
        request.segment_count = item->segment_count;
        request.start_utf8_byte = start;
        request.style = item->text_style;
        request.direction = item->direction;
        request.region.x = context->region.x;
        request.region.y = context->cursor_y;
        request.region.width = context->region.width;
        request.region.height = remaining;
        request.exclusions = context->active_floats;
        request.exclusion_count = context->active_float_count;
        request.inline_objects = objects;
        request.inline_object_count = inline_count;
        request.start_inline_object_index = start_inline;
        request.parts = parts;
        request.part_capacity = item->segment_count;
        memset(&metrics, 0, sizeof(metrics));
        metrics.struct_size = sizeof(metrics);
        status = text->measure_next(text->user_data, &request, &metrics);
        if (status == FW_STATUS_RESOURCE_LIMIT &&
            context->column_has_fragments != 0u &&
            request.region.height + 0.0001f < context->region.height) {
            status = fl_vp_next_region(context);
            continue;
        }
        if (status != FW_STATUS_OK) break;
        if (!fl_vp_inline_metrics_valid(item, &request, &metrics, total)) {
            status = FW_STATUS_INVALID_STATE;
            break;
        }
        if (start == 0u && metrics.reached_end == 0u &&
            (item->break_policy.keep_together != 0u ||
             metrics.line_count <
                fl_vp_line_rule(item->break_policy.orphans))) {
            if (context->column_has_fragments != 0u &&
                initial_region_retry == 0u) {
                status = fl_vp_next_region(context);
                if (status != FW_STATUS_OK) break;
                context->cursor_y += item->placement.margins.top;
                initial_region_retry = 1u;
                continue;
            }
            context->result->diagnostic_flags |=
                item->break_policy.keep_together != 0u ?
                    FW_FLOW_DIAGNOSTIC_KEEP_TOGETHER_RELAXED :
                    FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED;
        }
        if (metrics.reached_end == 0u &&
            fl_vp_line_rule(item->break_policy.widows) > 1u)
            context->result->diagnostic_flags |=
                FW_FLOW_DIAGNOSTIC_WIDOW_ORPHAN_RELAXED;
        status = fl_vp_emit_inline_parts(context, item, children, &request,
            &metrics);
        if (status != FW_STATUS_OK) break;
        context->cursor_y = fl_max(context->cursor_y,
            metrics.used_bounds.y + metrics.used_bounds.height);
        start = metrics.end_utf8_byte;
        start_inline = metrics.end_inline_object_index;
        if (metrics.reached_end != 0u) break;
        status = fl_vp_next_region(context);
    }
cleanup:
    free(parts);
    free(objects);
    free(children);
    return status;
}

static fw_status fl_vp_compose_object(fl_virtual_context *context,
    const fw_flow_item_v1 *item) {
    fw_child_measure_result_v1 measured;
    fw_flow_fragment_v1 fragment;
    fw_size_f32 size;
    char id[256];
    fw_status status;
    float remaining;
    if (++context->iterations > context->budget.iterations ||
        context->result->fragment_count >= context->budget.fragments ||
        item->placement.min_width > context->region.width)
        return FW_STATUS_RESOURCE_LIMIT;
    status = fl_vp_measure_object(context, item, &item->text_style,
        &measured, &size);
    if (status != FW_STATUS_OK) return status;
    remaining = context->region.y + context->region.height -
        context->cursor_y;
    if (size.width > context->region.width || size.height > remaining) {
        if (context->column_has_fragments != 0u) {
            status = fl_vp_next_region(context);
            if (status != FW_STATUS_OK) return status;
            context->cursor_y += item->placement.margins.top;
            remaining = context->region.y + context->region.height -
                context->cursor_y;
        }
    }
    status = fl_vp_fit_object(item, &size, context->region.width, remaining);
    if (status != FW_STATUS_OK) return status;
    memset(&fragment, 0, sizeof(fragment));
    fragment.struct_size = sizeof(fragment);
    fragment.kind = measured.used_fallback != 0u ?
        FW_FLOW_FRAGMENT_PLACEHOLDER : FW_FLOW_FRAGMENT_OBJECT;
    status = fl_make_fragment_id(id, sizeof(id), item->id,
        context->request->layout_revision, context->ordinal++,
        &fragment.derived_fragment_id);
    if (status != FW_STATUS_OK) return status;
    fragment.source_item_id = item->id;
    fragment.content_kind = item->content_kind;
    fragment.page_index = context->page_index;
    fragment.column_index = context->column_index;
    fragment.bounds.x = context->region.x + item->placement.offset_x;
    fragment.bounds.y = context->cursor_y + item->placement.offset_y;
    fragment.bounds.width = size.width;
    fragment.bounds.height = size.height;
    fragment.clip = context->region;
    fragment.z = item->placement.z;
    fragment.layout_fingerprint_high = measured.fingerprint_high;
    fragment.layout_fingerprint_low = measured.fingerprint_low;
    status = fl_vp_emit(context, &fragment);
    if (status != FW_STATUS_OK) return status;
    context->cursor_y += size.height;
    context->max_layout_bottom = fl_max(context->max_layout_bottom,
        context->cursor_y);
    return FW_STATUS_OK;
}

static fw_status fl_vp_compose_overlay(fl_virtual_context *context,
    const fw_flow_item_v1 *item) {
    fl_anchor_record *anchor = fl_vp_find_anchor(context,
        item->overlay_anchor_item_id);
    fw_child_measure_result_v1 measured;
    fw_flow_fragment_v1 fragment;
    fw_size_f32 size;
    char id[256];
    fw_status status;
    if (anchor == NULL || anchor->valid == 0u ||
        anchor->page_index != context->page_index)
        return FW_STATUS_UNSUPPORTED;
    if (++context->iterations > context->budget.iterations ||
        context->result->fragment_count >= context->budget.fragments)
        return FW_STATUS_RESOURCE_LIMIT;
    status = fl_vp_measure_object(context, item, &item->text_style,
        &measured, &size);
    if (status != FW_STATUS_OK) return status;
    status = fl_vp_fit_object(item, &size, anchor->clip.width,
        anchor->clip.height);
    if (status != FW_STATUS_OK) return status;
    memset(&fragment, 0, sizeof(fragment));
    fragment.struct_size = sizeof(fragment);
    fragment.kind = measured.used_fallback != 0u ?
        FW_FLOW_FRAGMENT_PLACEHOLDER : FW_FLOW_FRAGMENT_OBJECT;
    status = fl_make_fragment_id(id, sizeof(id), item->id,
        context->request->layout_revision, context->ordinal++,
        &fragment.derived_fragment_id);
    if (status != FW_STATUS_OK) return status;
    fragment.source_item_id = item->id;
    fragment.content_kind = item->content_kind;
    fragment.page_index = anchor->page_index;
    fragment.column_index = anchor->column_index;
    fragment.bounds.x = anchor->bounds.x + item->placement.margins.left +
        item->placement.offset_x;
    fragment.bounds.y = anchor->bounds.y + item->placement.margins.top +
        item->placement.offset_y;
    fragment.bounds.width = size.width;
    fragment.bounds.height = size.height;
    fragment.clip = anchor->clip;
    fragment.z = item->placement.z;
    fragment.layout_fingerprint_high = measured.fingerprint_high;
    fragment.layout_fingerprint_low = measured.fingerprint_low;
    fragment.flags = FW_FLOW_FRAGMENT_FLAG_OVERLAY;
    status = fl_vp_emit(context, &fragment);
    if (status != FW_STATUS_OK) return status;
    context->max_layout_bottom = fl_max(context->max_layout_bottom,
        fragment.bounds.y + fragment.bounds.height);
    return FW_STATUS_OK;
}

static fw_status fl_vp_compose_float(fl_virtual_context *context,
    const fw_flow_item_v1 *item) {
    fw_child_measure_result_v1 measured;
    fw_flow_fragment_v1 fragment;
    fw_text_exclusion_rect_v1 exclusion;
    fw_size_f32 size;
    char id[256];
    fw_status status;
    const float horizontal_margins = item->placement.margins.left +
        item->placement.margins.right;
    const float vertical_margin = item->placement.margins.bottom;
    float candidate_y;
    float left;
    float right;
    float next_bottom;
    float outer_x;
    int place_left;
    fl_vp_expire_floats(context);
    if (++context->iterations > context->budget.iterations ||
        context->result->fragment_count >= context->budget.fragments ||
        context->active_float_count >= context->active_float_capacity ||
        horizontal_margins >= context->region.width)
        return FW_STATUS_RESOURCE_LIMIT;
    status = fl_vp_measure_object(context, item, &item->text_style,
        &measured, &size);
    if (status != FW_STATUS_OK) return status;
    status = fl_vp_fit_object(item, &size,
        context->region.width - horizontal_margins,
        context->region.height - vertical_margin);
    if (status != FW_STATUS_OK) return status;
    candidate_y = context->cursor_y;
    for (;;) {
        const float region_bottom =
            context->region.y + context->region.height;
        const float available_height = region_bottom - candidate_y -
            item->placement.offset_y - vertical_margin;
        const float outer_bottom = candidate_y + item->placement.offset_y +
            size.height + vertical_margin;
        if (outer_bottom > region_bottom + 0.0001f) {
            if (context->column_has_fragments != 0u) {
                status = fl_vp_next_region(context);
                if (status != FW_STATUS_OK) return status;
                context->cursor_y += item->placement.margins.top;
                candidate_y = context->cursor_y;
                continue;
            }
            status = fl_vp_fit_object(item, &size,
                context->region.width - horizontal_margins,
                available_height);
            if (status != FW_STATUS_OK) return status;
            continue;
        }
        fl_vp_available_span(context,
            candidate_y + item->placement.offset_y, outer_bottom,
            &left, &right, &next_bottom);
        if (right - left + 0.0001f >=
            horizontal_margins + size.width) break;
        if (next_bottom <= candidate_y + 0.0001f ||
            next_bottom > region_bottom + 0.0001f)
            return FW_STATUS_RESOURCE_LIMIT;
        candidate_y = next_bottom;
    }
    place_left =
        (item->placement.mode == FW_FLOW_PLACE_FLOAT_START &&
         item->direction != FW_TEXT_DIRECTION_RTL) ||
        (item->placement.mode == FW_FLOW_PLACE_FLOAT_END &&
         item->direction == FW_TEXT_DIRECTION_RTL);
    outer_x = place_left ? left :
        right - horizontal_margins - size.width;
    memset(&fragment, 0, sizeof(fragment));
    fragment.struct_size = sizeof(fragment);
    fragment.kind = measured.used_fallback != 0u ?
        FW_FLOW_FRAGMENT_PLACEHOLDER : FW_FLOW_FRAGMENT_OBJECT;
    status = fl_make_fragment_id(id, sizeof(id), item->id,
        context->request->layout_revision, context->ordinal++,
        &fragment.derived_fragment_id);
    if (status != FW_STATUS_OK) return status;
    fragment.source_item_id = item->id;
    fragment.content_kind = item->content_kind;
    fragment.page_index = context->page_index;
    fragment.column_index = context->column_index;
    fragment.bounds.x = outer_x + item->placement.margins.left +
        item->placement.offset_x;
    fragment.bounds.y = candidate_y + item->placement.offset_y;
    fragment.bounds.width = size.width;
    fragment.bounds.height = size.height;
    fragment.clip = context->region;
    fragment.z = item->placement.z;
    fragment.layout_fingerprint_high = measured.fingerprint_high;
    fragment.layout_fingerprint_low = measured.fingerprint_low;
    status = fl_vp_emit(context, &fragment);
    if (status != FW_STATUS_OK) return status;
    memset(&exclusion, 0, sizeof(exclusion));
    exclusion.struct_size = sizeof(exclusion);
    exclusion.rect.x = outer_x + item->placement.offset_x;
    exclusion.rect.y = candidate_y + item->placement.offset_y;
    exclusion.rect.width = horizontal_margins + size.width;
    exclusion.rect.height = size.height + vertical_margin;
    context->active_floats[context->active_float_count++] = exclusion;
    context->max_layout_bottom = fl_max(context->max_layout_bottom,
        fl_vp_rect_bottom(&exclusion.rect));
    return FW_STATUS_OK;
}

static fw_status fl_vp_keep_item_height(fl_virtual_context *context,
    const fw_flow_item_v1 *item, float *out_height, int *out_supported) {
    *out_height = 0.0f;
    *out_supported = 1;
    if (item->placement.mode == FW_FLOW_PLACE_INLINE ||
        item->placement.mode == FW_FLOW_PLACE_FLOAT_START ||
        item->placement.mode == FW_FLOW_PLACE_FLOAT_END ||
        item->placement.mode == FW_FLOW_PLACE_OVERLAY) {
        *out_supported = 0;
        return FW_STATUS_OK;
    }
    if (item->kind == FW_FLOW_ITEM_OBJECT) {
        fw_child_measure_result_v1 measured;
        fw_size_f32 size;
        fw_status status;
        if (++context->iterations > context->budget.iterations)
            return FW_STATUS_RESOURCE_LIMIT;
        status = fl_vp_measure_object(context, item, &item->text_style,
            &measured, &size);
        if (status != FW_STATUS_OK) return status;
        status = fl_vp_fit_object(item, &size, context->region.width,
            context->region.height);
        if (status != FW_STATUS_OK) {
            *out_supported = 0;
            return FW_STATUS_OK;
        }
        *out_height = size.height;
        return FW_STATUS_OK;
    }
    if (fl_vp_has_inline_segments(item)) {
        *out_supported = 0;
        return FW_STATUS_OK;
    }
    {
        const fw_text_fragment_service_v1 *text = context->services->text;
        const size_t total = fl_vp_paragraph_bytes(item);
        fw_text_fragment_request_v1 request;
        fw_text_fragment_metrics_v1 metrics;
        fw_status status;
        if (!fl_vp_text_service_base_valid(text)) return FW_STATUS_INVALID_ARGUMENT;
        memset(&request, 0, sizeof(request));
        request.struct_size = sizeof(request);
        request.paragraph_id = item->id;
        request.segments = item->segments;
        request.segment_count = item->segment_count;
        request.style = item->text_style;
        request.direction = item->direction;
        request.region = context->region;
        request.region.y = context->content.y;
        request.region.height = context->content.height;
        memset(&metrics, 0, sizeof(metrics));
        metrics.struct_size = sizeof(metrics);
        if (++context->iterations > context->budget.iterations)
            return FW_STATUS_RESOURCE_LIMIT;
        status = text->measure_next(text->user_data, &request, &metrics);
        if (status != FW_STATUS_OK) return status;
        if (!fl_vp_metrics_valid(item, &request, total, &metrics))
            return FW_STATUS_INVALID_STATE;
        if (metrics.reached_end == 0u) {
            *out_supported = 0;
            return FW_STATUS_OK;
        }
        *out_height = metrics.used_bounds.height;
    }
    return FW_STATUS_OK;
}

static size_t fl_vp_next_top_level(
    const fw_flow_layout_request_v1 *request, size_t index) {
    for (++index; index < request->item_count; ++index) {
        if (request->items[index].placement.mode != FW_FLOW_PLACE_INLINE)
            return index;
    }
    return request->item_count;
}

static fw_status fl_vp_apply_keep_chain(fl_virtual_context *context,
    size_t start_index) {
    const fw_flow_layout_request_v1 *request = context->request;
    size_t index = start_index;
    uint32_t inspected = 0u;
    float height = 0.0f;
    float previous_bottom = 0.0f;
    fw_status status;
    if (context->continuous != 0u ||
        request->items[start_index].break_policy.keep_with_next == 0u)
        return FW_STATUS_OK;
    for (;;) {
        const fw_flow_item_v1 *item = &request->items[index];
        float item_height;
        int supported;
        if (++inspected > context->budget.backtracks) {
            context->result->diagnostic_flags |=
                FW_FLOW_DIAGNOSTIC_BACKTRACK_LIMIT |
                FW_FLOW_DIAGNOSTIC_KEEP_WITH_NEXT_RELAXED;
            return FW_STATUS_OK;
        }
        status = fl_vp_keep_item_height(context, item, &item_height,
            &supported);
        if (status != FW_STATUS_OK) return status;
        if (!supported) {
            context->result->diagnostic_flags |=
                FW_FLOW_DIAGNOSTIC_KEEP_WITH_NEXT_RELAXED;
            return FW_STATUS_OK;
        }
        height += fl_max(previous_bottom, item->placement.margins.top) +
            item_height;
        previous_bottom = item->placement.margins.bottom;
        if (item->break_policy.keep_with_next == 0u) break;
        index = fl_vp_next_top_level(request, index);
        if (index >= request->item_count) break;
        if (request->items[index].break_policy.break_before != 0u ||
            item->break_policy.break_after != 0u) {
            context->result->diagnostic_flags |=
                FW_FLOW_DIAGNOSTIC_KEEP_WITH_NEXT_RELAXED;
            return FW_STATUS_OK;
        }
    }
    height += previous_bottom;
    if (height > context->region.height + 0.0001f) {
        context->result->diagnostic_flags |=
            FW_FLOW_DIAGNOSTIC_KEEP_WITH_NEXT_RELAXED;
        return FW_STATUS_OK;
    }
    if (height > context->region.y + context->region.height -
            context->cursor_y + 0.0001f &&
        context->column_has_fragments != 0u)
        return fl_vp_next_region(context);
    return FW_STATUS_OK;
}

static fw_status fl_vp_supported_slice(
    const fw_flow_layout_request_v1 *request) {
    size_t item_index;
    for (item_index = 0u; item_index < request->item_count; ++item_index) {
        const fw_flow_item_v1 *item = &request->items[item_index];
        if (item->placement.mode == FW_FLOW_PLACE_INLINE ||
            item->placement.mode == FW_FLOW_PLACE_BLOCK) continue;
        if (item->kind == FW_FLOW_ITEM_OBJECT &&
            (item->placement.mode == FW_FLOW_PLACE_FLOAT_START ||
             item->placement.mode == FW_FLOW_PLACE_FLOAT_END)) continue;
        if (item->kind == FW_FLOW_ITEM_OBJECT &&
            item->placement.mode == FW_FLOW_PLACE_OVERLAY) continue;
        if (item->placement.mode != FW_FLOW_PLACE_BLOCK)
            return FW_STATUS_UNSUPPORTED;
    }
    return FW_STATUS_OK;
}

static fw_status fl_compose_paged_blocks(
    const fw_flow_layout_request_v1 *request,
    const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink,
    fw_flow_layout_result_v1 *out_result) {
    fl_virtual_context context;
    fw_status status;
    fw_status close_status;
    size_t index;
    size_t float_definitions = 0u;
    status = fl_vp_supported_slice(request);
    if (status != FW_STATUS_OK) return status;
    for (index = 0u; index < request->item_count; ++index) {
        const fw_flow_item_v1 *item = &request->items[index];
        if (item->kind == FW_FLOW_ITEM_PARAGRAPH &&
            fl_vp_has_inline_segments(item) &&
            !fl_vp_text_service_inline_valid(services->text))
            return FW_STATUS_UNSUPPORTED;
    }
    memset(&context, 0, sizeof(context));
    context.request = request;
    context.services = services;
    context.sink = sink;
    context.result = out_result;
    context.budget = fl_resolve_budget(&request->budget);
    context.page_seed = fl_request_hash(request);
    context.plan_hash = context.page_seed;
    context.continuous = request->page_template.mode == FW_FLOW_CONTINUOUS;
    context.column_count = request->page_template.mode == FW_FLOW_COLUMNS ?
        request->page_template.column_count : 1u;
    context.column_gap = request->page_template.mode == FW_FLOW_COLUMNS ?
        request->page_template.column_gap : 0.0f;
    context.content.x = request->page_template.margins.left;
    context.content.y = request->page_template.margins.top;
    context.content.width = request->page_template.page_size.width -
        request->page_template.margins.left -
        request->page_template.margins.right;
    context.content.height = request->page_template.page_size.height -
        request->page_template.margins.top -
        request->page_template.margins.bottom;
    for (index = 0u; index < request->item_count; ++index) {
        const fw_flow_placement_mode mode =
            request->items[index].placement.mode;
        if (mode == FW_FLOW_PLACE_FLOAT_START ||
            mode == FW_FLOW_PLACE_FLOAT_END) ++float_definitions;
    }
    context.active_float_capacity = float_definitions;
    if (context.active_float_capacity > context.budget.floats)
        context.active_float_capacity = context.budget.floats;
    if (context.active_float_capacity != 0u) {
        if (context.active_float_capacity >
            SIZE_MAX / sizeof(*context.active_floats))
            return FW_STATUS_RESOURCE_LIMIT;
        context.active_floats = (fw_text_exclusion_rect_v1 *)calloc(
            context.active_float_capacity, sizeof(*context.active_floats));
        if (context.active_floats == NULL) return FW_STATUS_OUT_OF_MEMORY;
    }
    context.anchor_count = request->item_count;
    if (context.anchor_count != 0u) {
        if (context.anchor_count > SIZE_MAX / sizeof(*context.anchors)) {
            free(context.active_floats);
            return FW_STATUS_RESOURCE_LIMIT;
        }
        context.anchors = (fl_anchor_record *)calloc(context.anchor_count,
            sizeof(*context.anchors));
        if (context.anchors == NULL) {
            free(context.active_floats);
            return FW_STATUS_OUT_OF_MEMORY;
        }
        for (index = 0u; index < context.anchor_count; ++index)
            context.anchors[index].item_id = request->items[index].id;
    }
    status = fl_vp_open_page(&context);
    if (status == FW_STATUS_OK) {
        for (index = 0u; index < request->item_count; ++index) {
            const fw_flow_item_v1 *item = &request->items[index];
            if (item->placement.mode == FW_FLOW_PLACE_INLINE) continue;
            if (item->break_policy.break_before != 0u) {
                status = fl_vp_apply_explicit_break(&context,
                    FW_FLOW_FRAGMENT_FLAG_BREAK_BEFORE);
                if (status != FW_STATUS_OK) break;
            }
            status = fl_vp_apply_keep_chain(&context, index);
            if (status != FW_STATUS_OK) break;
            if (item->placement.mode != FW_FLOW_PLACE_OVERLAY)
                context.cursor_y += fl_max(context.previous_bottom,
                    item->placement.margins.top);
            if (item->placement.mode == FW_FLOW_PLACE_FLOAT_START ||
                item->placement.mode == FW_FLOW_PLACE_FLOAT_END) {
                context.previous_bottom = 0.0f;
                status = fl_vp_compose_float(&context, item);
            } else if (item->placement.mode == FW_FLOW_PLACE_OVERLAY) {
                status = fl_vp_compose_overlay(&context, item);
            } else if (item->kind == FW_FLOW_ITEM_PARAGRAPH)
                status = fl_vp_compose_paragraph(&context, item);
            else
                status = fl_vp_compose_object(&context, item);
            if (status != FW_STATUS_OK) break;
            if (item->placement.mode == FW_FLOW_PLACE_BLOCK)
                context.previous_bottom = item->placement.margins.bottom;
            if (item->break_policy.break_after != 0u &&
                fl_vp_next_top_level(request, index) <
                    request->item_count) {
                status = fl_vp_apply_explicit_break(&context,
                    FW_FLOW_FRAGMENT_FLAG_BREAK_AFTER_PREVIOUS);
                if (status != FW_STATUS_OK) break;
            }
        }
    }
    close_status = fl_vp_close_page(&context);
    if (status == FW_STATUS_OK && close_status != FW_STATUS_OK)
        status = close_status;
    free(context.anchors);
    free(context.active_floats);
    out_result->continuous_extent.width =
        request->page_template.page_size.width;
    out_result->continuous_extent.height = context.continuous != 0u ?
        fl_max(context.cursor_y + context.previous_bottom,
            context.max_layout_bottom) +
            request->page_template.margins.bottom :
        (float)out_result->page_count *
            request->page_template.page_size.height;
    if (context.continuous == 0u && out_result->page_count > 1u) {
        out_result->continuous_extent.height +=
            (float)(out_result->page_count - 1u) *
                request->page_template.page_gap;
    }
    out_result->plan_key_high = context.plan_hash.high;
    out_result->plan_key_low = context.plan_hash.low;
    out_result->complete = status == FW_STATUS_OK;
    return status;
}

fw_status fl_compose_continuous(
    const fw_flow_layout_request_v1 *request,
    const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink,
    fw_flow_layout_result_v1 *out_result) {
    if (request == NULL || request->page_template.mode != FW_FLOW_CONTINUOUS)
        return FW_STATUS_INVALID_ARGUMENT;
    return fl_compose_paged_blocks(request, services, sink, out_result);
}

fw_status fl_compose_virtual_pages(
    const fw_flow_layout_request_v1 *request,
    const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink,
    fw_flow_layout_result_v1 *out_result) {
    if (request == NULL || request->page_template.mode != FW_FLOW_VIRTUAL_PAGES)
        return FW_STATUS_INVALID_ARGUMENT;
    return fl_compose_paged_blocks(request, services, sink, out_result);
}

fw_status fl_compose_columns(
    const fw_flow_layout_request_v1 *request,
    const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink,
    fw_flow_layout_result_v1 *out_result) {
    if (request == NULL || request->page_template.mode != FW_FLOW_COLUMNS)
        return FW_STATUS_INVALID_ARGUMENT;
    return fl_compose_paged_blocks(request, services, sink, out_result);
}
