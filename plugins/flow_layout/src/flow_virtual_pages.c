/* SPDX-License-Identifier: MPL-2.0 */
#include "flow_internal.h"

#include <math.h>
#include <stdio.h>
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
    float cursor_y;
    float previous_bottom;
    float column_gap;
} fl_virtual_context;

static int fl_vp_bool(uint32_t value) {
    return value <= 1u;
}

static int fl_vp_finite(float value) {
    return isfinite(value);
}

static int fl_vp_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
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
    return fl_vp_next_page(context);
}

static fw_status fl_vp_emit(fl_virtual_context *context,
    const fw_flow_fragment_v1 *fragment) {
    const fw_status status = fl_emit(context->sink, fragment,
        context->result, &context->plan_hash);
    if (status == FW_STATUS_OK) {
        context->page_has_fragments = 1u;
        context->column_has_fragments = 1u;
    }
    return status;
}

static int fl_vp_metrics_valid(const fw_flow_item_v1 *item,
    const fw_text_fragment_request_v1 *request, size_t total,
    const fw_text_fragment_metrics_v1 *metrics) {
    return metrics->struct_size >= sizeof(*metrics) &&
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

static fw_status fl_vp_compose_paragraph(fl_virtual_context *context,
    const fw_flow_item_v1 *item) {
    const fw_text_fragment_service_v1 *text = context->services->text;
    const size_t total = fl_vp_paragraph_bytes(item);
    size_t start = 0u;
    uint32_t continuation = 0u;
    if (text == NULL || text->struct_size < sizeof(*text) ||
        text->measure_next == NULL) return FW_STATUS_INVALID_ARGUMENT;
    for (;;) {
        fw_text_fragment_request_v1 request;
        fw_text_fragment_metrics_v1 metrics;
        fw_flow_fragment_v1 fragment;
        char id[256];
        fw_status status;
        float remaining = context->region.y + context->region.height -
            context->cursor_y;
        const float line_height = item->text_style.font_size *
            item->text_style.line_height_multiplier;
        if (++context->iterations > context->budget.iterations ||
            context->result->fragment_count >= context->budget.fragments)
            return FW_STATUS_RESOURCE_LIMIT;
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
    const fw_flow_item_v1 *item, fw_child_measure_result_v1 *measured,
    fw_size_f32 *size) {
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
    request.constraints.em_size = item->text_style.font_size;
    request.constraints.line_height = item->text_style.font_size *
        item->text_style.line_height_multiplier;
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
    status = fl_vp_measure_object(context, item, &measured, &size);
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
    return FW_STATUS_OK;
}

static fw_status fl_vp_supported_slice(
    const fw_flow_layout_request_v1 *request) {
    size_t item_index;
    for (item_index = 0u; item_index < request->item_count; ++item_index) {
        const fw_flow_item_v1 *item = &request->items[item_index];
        size_t segment_index;
        if (item->break_policy.break_before != 0u ||
            item->break_policy.break_after != 0u ||
            item->break_policy.keep_together != 0u ||
            item->break_policy.keep_with_next != 0u)
            return FW_STATUS_UNSUPPORTED;
        if (item->placement.mode == FW_FLOW_PLACE_INLINE) continue;
        if (item->placement.mode != FW_FLOW_PLACE_BLOCK)
            return FW_STATUS_UNSUPPORTED;
        if (item->kind != FW_FLOW_ITEM_PARAGRAPH) continue;
        for (segment_index = 0u;
             segment_index < item->segment_count; ++segment_index) {
            if (item->segments[segment_index].kind ==
                FW_FLOW_SEGMENT_OBJECT) return FW_STATUS_UNSUPPORTED;
        }
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
    status = fl_vp_supported_slice(request);
    if (status != FW_STATUS_OK) return status;
    memset(&context, 0, sizeof(context));
    context.request = request;
    context.services = services;
    context.sink = sink;
    context.result = out_result;
    context.budget = fl_resolve_budget(&request->budget);
    context.page_seed = fl_request_hash(request);
    context.plan_hash = context.page_seed;
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
    status = fl_vp_open_page(&context);
    if (status == FW_STATUS_OK) {
        for (index = 0u; index < request->item_count; ++index) {
            const fw_flow_item_v1 *item = &request->items[index];
            if (item->placement.mode == FW_FLOW_PLACE_INLINE) continue;
            context.cursor_y += fl_max(context.previous_bottom,
                item->placement.margins.top);
            if (item->kind == FW_FLOW_ITEM_PARAGRAPH)
                status = fl_vp_compose_paragraph(&context, item);
            else
                status = fl_vp_compose_object(&context, item);
            if (status != FW_STATUS_OK) break;
            context.previous_bottom = item->placement.margins.bottom;
        }
    }
    close_status = fl_vp_close_page(&context);
    if (status == FW_STATUS_OK && close_status != FW_STATUS_OK)
        status = close_status;
    out_result->continuous_extent.width =
        request->page_template.page_size.width;
    out_result->continuous_extent.height =
        (float)out_result->page_count *
            request->page_template.page_size.height;
    if (out_result->page_count > 1u) {
        out_result->continuous_extent.height +=
            (float)(out_result->page_count - 1u) *
                request->page_template.page_gap;
    }
    out_result->plan_key_high = context.plan_hash.high;
    out_result->plan_key_low = context.plan_hash.low;
    out_result->complete = status == FW_STATUS_OK;
    return status;
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
