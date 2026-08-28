/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/child_measure_service.h>
#include <facetwire/flow_layout.h>
#include <facetwire/renderer.h>
#include <facetwire/text_fragment_service.h>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FL_MAGIC UINT32_C(0x464c5731)
#define FL_MAX_STRING_BYTES 8192u
#define FL_DEFAULT_MAX_ITEMS 4096u
#define FL_DEFAULT_MAX_SEGMENTS 16384u
#define FL_DEFAULT_MAX_PAGES 1024u
#define FL_DEFAULT_MAX_FRAGMENTS 65536u
#define FL_DEFAULT_MAX_ITERATIONS 131072u

typedef struct fl_context {
    uint32_t magic;
    fw_host_api_v1 host;
} fl_context;

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

static const fw_capability_descriptor_v1 fl_capabilities[] = {{
    sizeof(fw_capability_descriptor_v1),
    FW_STRING_VIEW_LITERAL(FW_FLOW_LAYOUT_CAPABILITY_ID),
    FW_STRING_VIEW_LITERAL(FW_FLOW_LAYOUT_CAPABILITY_KIND),
    FW_RENDERER_FLAG_DETERMINISTIC | FW_RENDERER_FLAG_HEADLESS,
}};

static const fw_plugin_descriptor_v1 fl_descriptor = {
    sizeof(fw_plugin_descriptor_v1), FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.flow-layout"),
    FW_STRING_VIEW_LITERAL("FacetWire Flow Layout Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    fl_capabilities, sizeof(fl_capabilities) / sizeof(fl_capabilities[0]),
};

static const char fl_parameter_schema[] =
    "{\"schemaVersion\":1,\"implementationStatus\":\"experimental\","
    "\"supportedPageModes\":[\"continuous\"],"
    "\"supportedPlacements\":[\"block\"],"
    "\"parameters\":[{\"id\":\"pageMode\",\"type\":\"enum\","
    "\"default\":\"continuous\",\"values\":[\"continuous\"]}]}";

static int fl_context_valid(fw_plugin_handle plugin) {
    const fl_context *context = (const fl_context *)plugin;
    return context != NULL && context->magic == FL_MAGIC;
}

static fw_string_view fl_view(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static int fl_string_shape(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int fl_string_equal(fw_string_view left, fw_string_view right) {
    return left.length == right.length &&
        (left.length == 0u || memcmp(left.data, right.data, left.length) == 0);
}

static int fl_string_literal(fw_string_view value, const char *literal) {
    const size_t length = strlen(literal);
    return value.length == length &&
        (length == 0u || memcmp(value.data, literal, length) == 0);
}

static int fl_utf8_valid(fw_string_view value) {
    size_t i = 0u;
    while (i < value.length) {
        const unsigned char first = (unsigned char)value.data[i++];
        size_t count;
        uint32_t cp;
        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            count = 1u;
            cp = first & 0x1fu;
        } else if (first >= 0xe0u && first <= 0xefu) {
            count = 2u;
            cp = first & 0x0fu;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            count = 3u;
            cp = first & 0x07u;
        } else {
            return 0;
        }
        if (count > value.length - i) return 0;
        while (count-- != 0u) {
            const unsigned char next = (unsigned char)value.data[i++];
            if ((next & 0xc0u) != 0x80u) return 0;
            cp = (cp << 6u) | (next & 0x3fu);
        }
        if ((cp >= 0xd800u && cp <= 0xdfffu) || cp > 0x10ffffu ||
            (cp < 0x800u && first >= 0xe0u) ||
            (cp < 0x10000u && first >= 0xf0u)) return 0;
    }
    return 1;
}

static int fl_valid_string(fw_string_view value, int required) {
    return fl_string_shape(value) && (!required || value.length != 0u) &&
        value.length <= FL_MAX_STRING_BYTES && fl_utf8_valid(value);
}

static int fl_bool(uint32_t value) { return value <= 1u; }

static int fl_finite(float value) { return isfinite(value); }

static int fl_unit(float value) {
    return isfinite(value) && value >= 0.0f && value <= 1.0f;
}

static int fl_color_valid(fw_color_rgba_f32 value) {
    return fl_unit(value.red) && fl_unit(value.green) &&
        fl_unit(value.blue) && fl_unit(value.alpha);
}

static int fl_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
}

static int fl_insets_valid(fw_edge_insets_f32 value) {
    return fl_nonnegative(value.left) && fl_nonnegative(value.top) &&
        fl_nonnegative(value.right) && fl_nonnegative(value.bottom);
}

static int fl_item_kind_valid(fw_flow_item_kind value) {
    return value == FW_FLOW_ITEM_PARAGRAPH || value == FW_FLOW_ITEM_OBJECT;
}

static int fl_placement_valid(fw_flow_placement_mode value) {
    return value >= FW_FLOW_PLACE_BLOCK && value <= FW_FLOW_PLACE_OVERLAY;
}

static int fl_page_mode_valid(fw_flow_page_mode value) {
    return value >= FW_FLOW_CONTINUOUS && value <= FW_FLOW_COLUMNS;
}

static uint32_t fl_budget_value(uint32_t value, uint32_t fallback) {
    return value == 0u ? fallback : value;
}

static fl_budget fl_resolve_budget(const fw_flow_budget_v1 *value) {
    fl_budget result;
    result.items = fl_budget_value(value->max_items, FL_DEFAULT_MAX_ITEMS);
    result.segments = fl_budget_value(value->max_segments,
        FL_DEFAULT_MAX_SEGMENTS);
    result.pages = fl_budget_value(value->max_pages, FL_DEFAULT_MAX_PAGES);
    result.fragments = fl_budget_value(value->max_fragments,
        FL_DEFAULT_MAX_FRAGMENTS);
    result.iterations = fl_budget_value(value->max_iterations,
        FL_DEFAULT_MAX_ITERATIONS);
    return result;
}

static const fw_flow_item_v1 *fl_find_item(
    const fw_flow_layout_request_v1 *request, fw_string_view id) {
    size_t i;
    for (i = 0u; i < request->item_count; ++i) {
        if (fl_string_equal(request->items[i].id, id))
            return &request->items[i];
    }
    return NULL;
}

static fw_status fl_validate_request(fw_plugin_handle plugin,
    const fw_flow_layout_request_v1 *request, const char **out_key) {
    fl_budget budget;
    size_t total_segments = 0u;
    size_t i;
    *out_key = "flow.invalid_argument";
    if (!fl_context_valid(plugin) || request == NULL ||
        request->struct_size < sizeof(*request) ||
        request->page_template.struct_size < sizeof(request->page_template) ||
        request->budget.struct_size < sizeof(request->budget) ||
        request->target.struct_size < sizeof(request->target)) {
        *out_key = "flow.invalid_struct_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!fl_valid_string(request->flow_id, 1) ||
        !fl_valid_string(request->profile_key, 0) ||
        (request->item_count != 0u && request->items == NULL)) {
        *out_key = "flow.invalid_string_or_array";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!fl_page_mode_valid(request->page_template.mode) ||
        request->page_template.column_count == 0u ||
        !fl_nonnegative(request->page_template.page_size.width) ||
        !fl_nonnegative(request->page_template.page_size.height) ||
        request->page_template.page_size.width == 0.0f ||
        request->page_template.page_size.height == 0.0f ||
        !fl_insets_valid(request->page_template.margins) ||
        !fl_nonnegative(request->page_template.column_gap) ||
        !fl_nonnegative(request->page_template.page_gap) ||
        !fl_nonnegative(request->page_template.minimum_text_width) ||
        request->page_template.margins.left +
            request->page_template.margins.right >=
            request->page_template.page_size.width ||
        request->page_template.margins.top +
            request->page_template.margins.bottom >=
            request->page_template.page_size.height ||
        (request->page_template.mode == FW_FLOW_COLUMNS &&
            request->page_template.column_count < 2u) ||
        (request->page_template.mode != FW_FLOW_COLUMNS &&
            request->page_template.column_count != 1u) ||
        request->page_template.minimum_text_width >
            request->page_template.page_size.width -
                request->page_template.margins.left -
                request->page_template.margins.right) {
        *out_key = "flow.invalid_page_template";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->target.medium < FW_RENDER_MEDIUM_SCREEN ||
        request->target.medium > FW_RENDER_MEDIUM_HEADLESS ||
        !fl_nonnegative(request->target.device_pixel_ratio) ||
        request->target.device_pixel_ratio == 0.0f ||
        !fl_nonnegative(request->target.font_scale) ||
        request->target.font_scale == 0.0f ||
        !fl_bool(request->target.prefers_dark) ||
        !fl_bool(request->target.high_contrast) ||
        !fl_bool(request->target.reduce_motion) ||
        !fl_bool(request->target.supports_alpha)) {
        *out_key = "flow.invalid_target";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    budget = fl_resolve_budget(&request->budget);
    if (request->item_count > budget.items) {
        *out_key = "flow.item_budget_exceeded";
        return FW_STATUS_RESOURCE_LIMIT;
    }
    for (i = 0u; i < request->item_count; ++i) {
        const fw_flow_item_v1 *item = &request->items[i];
        size_t j;
        if (item->struct_size < sizeof(*item) ||
            item->placement.struct_size < sizeof(item->placement) ||
            item->break_policy.struct_size < sizeof(item->break_policy) ||
            !fl_valid_string(item->id, 1) ||
            !fl_item_kind_valid(item->kind) ||
            !fl_placement_valid(item->placement.mode) ||
            !fl_insets_valid(item->placement.margins) ||
            !fl_nonnegative(item->placement.requested_width) ||
            !fl_nonnegative(item->placement.requested_height) ||
            !fl_nonnegative(item->placement.min_width) ||
            !fl_nonnegative(item->placement.min_height) ||
            !fl_nonnegative(item->placement.max_width) ||
            !fl_nonnegative(item->placement.max_height) ||
            !fl_finite(item->placement.offset_x) ||
            !fl_finite(item->placement.offset_y) ||
            !fl_bool(item->placement.allow_scale_down) ||
            !fl_bool(item->placement.allow_scale_up) ||
            !fl_bool(item->break_policy.break_before) ||
            !fl_bool(item->break_policy.break_after) ||
            !fl_bool(item->break_policy.keep_together) ||
            !fl_bool(item->break_policy.keep_with_next) ||
            !fl_bool(item->decorative) ||
            (item->placement.max_width != 0.0f &&
                item->placement.max_width < item->placement.min_width) ||
            (item->placement.max_height != 0.0f &&
                item->placement.max_height < item->placement.min_height)) {
            *out_key = "flow.invalid_item";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        for (j = 0u; j < i; ++j) {
            if (fl_string_equal(item->id, request->items[j].id)) {
                *out_key = "flow.duplicate_item_id";
                return FW_STATUS_INVALID_ARGUMENT;
            }
        }
        if (item->kind == FW_FLOW_ITEM_PARAGRAPH) {
            size_t font_index;
            if (item->text_style.struct_size < sizeof(item->text_style) ||
                item->placement.mode != FW_FLOW_PLACE_BLOCK ||
                item->segment_count == 0u || item->segments == NULL ||
                item->direction > FW_TEXT_DIRECTION_RTL ||
                item->content_id.length != 0u ||
                item->content_kind.length != 0u ||
                item->text_style.font_family_count > 16u ||
                (item->text_style.font_family_count != 0u &&
                    item->text_style.font_families == NULL) ||
                !fl_valid_string(item->text_style.font_resource_id, 0) ||
                !fl_finite(item->text_style.font_size) ||
                item->text_style.font_size <= 0.0f ||
                item->text_style.font_weight < 1u ||
                item->text_style.font_weight > 1000u ||
                item->text_style.font_style > FW_TEXT_FONT_OBLIQUE ||
                !fl_finite(item->text_style.line_height_multiplier) ||
                item->text_style.line_height_multiplier < 0.5f ||
                !fl_finite(item->text_style.letter_spacing) ||
                !fl_color_valid(item->text_style.color) ||
                !fl_bool(item->text_style.has_background_color) ||
                (item->text_style.has_background_color != 0u &&
                    !fl_color_valid(item->text_style.background_color))) {
                *out_key = "flow.invalid_paragraph";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            for (font_index = 0u;
                 font_index < item->text_style.font_family_count;
                 ++font_index) {
                if (!fl_valid_string(
                        item->text_style.font_families[font_index], 1)) {
                    *out_key = "flow.invalid_font_family";
                    return FW_STATUS_INVALID_ARGUMENT;
                }
            }
        } else if (!fl_valid_string(item->content_id, 1) ||
            !fl_valid_string(item->content_kind, 1) ||
            item->segment_count != 0u || item->segments != NULL) {
            *out_key = "flow.invalid_object";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        if (item->segment_count > SIZE_MAX - total_segments) {
            *out_key = "flow.segment_budget_exceeded";
            return FW_STATUS_RESOURCE_LIMIT;
        }
        total_segments += item->segment_count;
        if (total_segments > budget.segments) {
            *out_key = "flow.segment_budget_exceeded";
            return FW_STATUS_RESOURCE_LIMIT;
        }
        for (j = 0u; j < item->segment_count; ++j) {
            const fw_flow_segment_v1 *segment = &item->segments[j];
            if (segment->struct_size < sizeof(*segment) ||
                (segment->kind != FW_FLOW_SEGMENT_TEXT &&
                 segment->kind != FW_FLOW_SEGMENT_OBJECT)) {
                *out_key = "flow.invalid_segment";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if (segment->kind == FW_FLOW_SEGMENT_TEXT) {
                if (!fl_valid_string(segment->text, 0) ||
                    segment->object_item_id.length != 0u ||
                    segment->baseline_mode >
                        FW_FLOW_BASELINE_TEXT_BOTTOM) {
                    *out_key = "flow.invalid_text_segment";
                    return FW_STATUS_INVALID_ARGUMENT;
                }
            } else {
                const fw_flow_item_v1 *referenced;
                if (segment->text.length != 0u ||
                    !fl_valid_string(segment->object_item_id, 1) ||
                    segment->baseline_mode >
                        FW_FLOW_BASELINE_TEXT_BOTTOM) {
                    *out_key = "flow.invalid_object_segment";
                    return FW_STATUS_INVALID_ARGUMENT;
                }
                referenced = fl_find_item(request, segment->object_item_id);
                if (referenced == NULL ||
                    referenced->kind != FW_FLOW_ITEM_OBJECT ||
                    referenced->placement.mode != FW_FLOW_PLACE_INLINE) {
                    *out_key = "flow.invalid_inline_reference";
                    return FW_STATUS_INVALID_ARGUMENT;
                }
            }
        }
    }
    for (i = 0u; i < request->item_count; ++i) {
        const fw_flow_item_v1 *candidate = &request->items[i];
        size_t references = 0u;
        size_t j;
        if (candidate->kind != FW_FLOW_ITEM_OBJECT ||
            candidate->placement.mode != FW_FLOW_PLACE_INLINE) continue;
        for (j = 0u; j < request->item_count; ++j) {
            const fw_flow_item_v1 *owner = &request->items[j];
            size_t k;
            for (k = 0u; k < owner->segment_count; ++k) {
                if (owner->segments[k].kind == FW_FLOW_SEGMENT_OBJECT &&
                    fl_string_equal(owner->segments[k].object_item_id,
                        candidate->id)) ++references;
            }
        }
        if (references != 1u) {
            *out_key = "flow.inline_object_ownership";
            return FW_STATUS_INVALID_ARGUMENT;
        }
    }
    *out_key = "flow.ok";
    return FW_STATUS_OK;
}

static void fl_hash_init(fl_hash *hash) {
    hash->high = UINT64_C(1469598103934665603);
    hash->low = UINT64_C(1099511628211) ^ UINT64_C(0x9e3779b97f4a7c15);
}

static void fl_hash_bytes(fl_hash *hash, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0u; i < length; ++i) {
        hash->high ^= bytes[i];
        hash->high *= UINT64_C(1099511628211);
        hash->low ^= (uint64_t)(bytes[i] + 0x9du);
        hash->low *= UINT64_C(14029467366897019727);
    }
}

static void fl_hash_u64(fl_hash *hash, uint64_t value) {
    uint32_t shift;
    for (shift = 0u; shift < 64u; shift += 8u) {
        const unsigned char byte = (unsigned char)(value >> shift);
        fl_hash_bytes(hash, &byte, 1u);
    }
}

static void fl_hash_f32(fl_hash *hash, float value) {
    uint32_t bits;
    uint32_t shift;
    if (value == 0.0f) value = 0.0f;
    memcpy(&bits, &value, sizeof(bits));
    for (shift = 0u; shift < 32u; shift += 8u) {
        const unsigned char byte = (unsigned char)(bits >> shift);
        fl_hash_bytes(hash, &byte, 1u);
    }
}

static void fl_hash_view(fl_hash *hash, fw_string_view value) {
    fl_hash_u64(hash, (uint64_t)value.length);
    fl_hash_bytes(hash, value.data, value.length);
}

static void fl_hash_insets(fl_hash *hash, fw_edge_insets_f32 value) {
    fl_hash_f32(hash, value.left);
    fl_hash_f32(hash, value.top);
    fl_hash_f32(hash, value.right);
    fl_hash_f32(hash, value.bottom);
}

static fl_hash fl_request_hash(const fw_flow_layout_request_v1 *request) {
    fl_hash hash;
    size_t i;
    fl_hash_init(&hash);
    fl_hash_view(&hash, request->flow_id);
    fl_hash_u64(&hash, request->document_revision);
    fl_hash_u64(&hash, request->layout_revision);
    fl_hash_view(&hash, request->profile_key);
    fl_hash_u64(&hash, request->page_template.mode);
    fl_hash_f32(&hash, request->page_template.page_size.width);
    fl_hash_f32(&hash, request->page_template.page_size.height);
    fl_hash_insets(&hash, request->page_template.margins);
    fl_hash_u64(&hash, request->page_template.column_count);
    fl_hash_f32(&hash, request->page_template.column_gap);
    fl_hash_f32(&hash, request->page_template.page_gap);
    fl_hash_f32(&hash, request->page_template.minimum_text_width);
    fl_hash_f32(&hash, request->target.device_pixel_ratio);
    fl_hash_f32(&hash, request->target.font_scale);
    fl_hash_u64(&hash, request->target.medium);
    fl_hash_u64(&hash, request->target.prefers_dark);
    fl_hash_u64(&hash, request->target.high_contrast);
    fl_hash_u64(&hash, request->target.reduce_motion);
    fl_hash_u64(&hash, request->target.supports_alpha);
    fl_hash_u64(&hash, (uint64_t)request->item_count);
    for (i = 0u; i < request->item_count; ++i) {
        const fw_flow_item_v1 *item = &request->items[i];
        size_t j;
        fl_hash_view(&hash, item->id);
        fl_hash_u64(&hash, item->kind);
        fl_hash_u64(&hash, item->placement.mode);
        fl_hash_insets(&hash, item->placement.margins);
        fl_hash_f32(&hash, item->placement.requested_width);
        fl_hash_f32(&hash, item->placement.requested_height);
        fl_hash_f32(&hash, item->placement.min_width);
        fl_hash_f32(&hash, item->placement.min_height);
        fl_hash_f32(&hash, item->placement.max_width);
        fl_hash_f32(&hash, item->placement.max_height);
        fl_hash_f32(&hash, item->placement.offset_x);
        fl_hash_f32(&hash, item->placement.offset_y);
        fl_hash_u64(&hash, (uint64_t)(int64_t)item->placement.z);
        fl_hash_u64(&hash, item->placement.allow_scale_down);
        fl_hash_u64(&hash, item->placement.allow_scale_up);
        fl_hash_u64(&hash, item->break_policy.break_before);
        fl_hash_u64(&hash, item->break_policy.break_after);
        fl_hash_u64(&hash, item->break_policy.keep_together);
        fl_hash_u64(&hash, item->break_policy.keep_with_next);
        fl_hash_u64(&hash, item->break_policy.orphans);
        fl_hash_u64(&hash, item->break_policy.widows);
        fl_hash_f32(&hash, item->text_style.font_size);
        fl_hash_u64(&hash, item->text_style.font_weight);
        fl_hash_u64(&hash, item->text_style.font_style);
        fl_hash_f32(&hash, item->text_style.line_height_multiplier);
        fl_hash_f32(&hash, item->text_style.letter_spacing);
        fl_hash_u64(&hash, item->direction);
        fl_hash_u64(&hash, (uint64_t)item->text_style.font_family_count);
        for (j = 0u; j < item->text_style.font_family_count; ++j)
            fl_hash_view(&hash, item->text_style.font_families[j]);
        fl_hash_view(&hash, item->text_style.font_resource_id);
        fl_hash_view(&hash, item->content_id);
        fl_hash_view(&hash, item->content_kind);
        for (j = 0u; j < item->segment_count; ++j) {
            fl_hash_u64(&hash, item->segments[j].kind);
            fl_hash_view(&hash, item->segments[j].text);
            fl_hash_view(&hash, item->segments[j].object_item_id);
        }
    }
    return hash;
}

static float fl_max(float left, float right) {
    return left > right ? left : right;
}

static float fl_min(float left, float right) {
    return left < right ? left : right;
}

static float fl_clamp_dimension(float value, float minimum, float maximum) {
    float result = fl_max(value, minimum);
    if (maximum > 0.0f) result = fl_min(result, maximum);
    return result;
}

static fw_status fl_emit(fw_flow_plan_sink_v1 const *sink,
    const fw_flow_fragment_v1 *fragment, fw_flow_layout_result_v1 *result,
    fl_hash *hash) {
    fw_status status = sink->emit_fragment(sink->user_data, fragment);
    if (status != FW_STATUS_OK) return FW_STATUS_SINK_REJECTED;
    ++result->fragment_count;
    if (fragment->kind == FW_FLOW_FRAGMENT_TEXT)
        ++result->text_fragment_count;
    else
        ++result->object_fragment_count;
    fl_hash_view(hash, fragment->source_item_id);
    fl_hash_u64(hash, fragment->kind);
    fl_hash_f32(hash, fragment->bounds.x);
    fl_hash_f32(hash, fragment->bounds.y);
    fl_hash_f32(hash, fragment->bounds.width);
    fl_hash_f32(hash, fragment->bounds.height);
    fl_hash_u64(hash, fragment->layout_fingerprint_high);
    fl_hash_u64(hash, fragment->layout_fingerprint_low);
    return FW_STATUS_OK;
}

static fw_status fl_make_fragment_id(char *buffer, size_t capacity,
    fw_string_view item_id, uint64_t revision, uint32_t ordinal,
    fw_string_view *out_view) {
    fl_hash item_hash;
    int written;
    fl_hash_init(&item_hash);
    fl_hash_view(&item_hash, item_id);
    written = snprintf(buffer, capacity, "vf:%016llx%016llx:%llu:%u",
        (unsigned long long)item_hash.high,
        (unsigned long long)item_hash.low,
        (unsigned long long)revision, ordinal);
    if (written < 0 || (size_t)written >= capacity)
        return FW_STATUS_RESOURCE_LIMIT;
    out_view->data = buffer;
    out_view->length = (size_t)written;
    return FW_STATUS_OK;
}

static size_t fl_paragraph_text_bytes(const fw_flow_item_v1 *item) {
    size_t total = 0u;
    size_t i;
    for (i = 0u; i < item->segment_count; ++i) {
        if (item->segments[i].kind == FW_FLOW_SEGMENT_TEXT)
            total += item->segments[i].text.length;
    }
    return total;
}

static int fl_paragraph_offset_boundary(const fw_flow_item_v1 *item,
    size_t offset) {
    size_t cursor = 0u;
    size_t i;
    for (i = 0u; i < item->segment_count; ++i) {
        const fw_string_view text = item->segments[i].text;
        if (offset == cursor) return 1;
        if (offset < cursor + text.length) {
            const unsigned char value = (unsigned char)text.data[offset - cursor];
            return (value & 0xc0u) != 0x80u;
        }
        cursor += text.length;
    }
    return offset == cursor;
}

static fw_status fl_compose_paragraph(const fw_flow_layout_request_v1 *request,
    const fw_flow_item_v1 *item, const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink, fw_rect_f32 content, float *cursor_y,
    uint32_t *ordinal, const fl_budget *budget,
    fw_flow_layout_result_v1 *result, fl_hash *hash) {
    const fw_text_fragment_service_v1 *text = services->text;
    size_t start = 0u;
    uint32_t iterations = 0u;
    uint32_t continuation = 0u;
    const size_t total_bytes = fl_paragraph_text_bytes(item);
    if (text == NULL || text->struct_size < sizeof(*text) ||
        text->measure_next == NULL) return FW_STATUS_INVALID_ARGUMENT;
    for (;;) {
        fw_text_fragment_request_v1 measure_request;
        fw_text_fragment_metrics_v1 metrics;
        fw_flow_fragment_v1 fragment;
        char id_buffer[256];
        fw_status status;
        const float remaining = content.y + content.height - *cursor_y;
        if (++iterations > budget->iterations ||
            result->fragment_count >= budget->fragments)
            return FW_STATUS_RESOURCE_LIMIT;
        if (remaining <= 0.0f) return FW_STATUS_RESOURCE_LIMIT;
        memset(&measure_request, 0, sizeof(measure_request));
        measure_request.struct_size = sizeof(measure_request);
        measure_request.paragraph_id = item->id;
        measure_request.segments = item->segments;
        measure_request.segment_count = item->segment_count;
        measure_request.start_utf8_byte = start;
        measure_request.style = item->text_style;
        measure_request.direction = item->direction;
        measure_request.region.x = content.x;
        measure_request.region.y = *cursor_y;
        measure_request.region.width = content.width;
        measure_request.region.height = remaining;
        memset(&metrics, 0, sizeof(metrics));
        metrics.struct_size = sizeof(metrics);
        status = text->measure_next(text->user_data, &measure_request,
            &metrics);
        if (status != FW_STATUS_OK) return status;
        if (metrics.struct_size < sizeof(metrics) ||
            !fl_bool(metrics.reached_end) ||
            !fl_finite(metrics.used_bounds.x) ||
            !fl_finite(metrics.used_bounds.y) ||
            !fl_nonnegative(metrics.used_bounds.width) ||
            !fl_nonnegative(metrics.used_bounds.height) ||
            metrics.used_bounds.x < measure_request.region.x ||
            metrics.used_bounds.y < measure_request.region.y ||
            metrics.used_bounds.x + metrics.used_bounds.width >
                measure_request.region.x + measure_request.region.width ||
            metrics.used_bounds.y + metrics.used_bounds.height >
                measure_request.region.y + measure_request.region.height ||
            metrics.end_utf8_byte < start ||
            metrics.end_utf8_byte > total_bytes ||
            !fl_paragraph_offset_boundary(item, metrics.end_utf8_byte) ||
            ((metrics.reached_end != 0u) !=
                (metrics.end_utf8_byte == total_bytes)) ||
            (metrics.reached_end == 0u && metrics.end_utf8_byte == start))
            return FW_STATUS_INVALID_STATE;
        memset(&fragment, 0, sizeof(fragment));
        fragment.struct_size = sizeof(fragment);
        fragment.kind = FW_FLOW_FRAGMENT_TEXT;
        status = fl_make_fragment_id(id_buffer, sizeof(id_buffer), item->id,
            request->layout_revision, (*ordinal)++,
            &fragment.derived_fragment_id);
        if (status != FW_STATUS_OK) return status;
        fragment.source_item_id = item->id;
        fragment.page_index = 0u;
        fragment.bounds = metrics.used_bounds;
        fragment.clip = content;
        fragment.z = item->placement.z;
        fragment.text_start_utf8_byte = start;
        fragment.text_end_utf8_byte = metrics.end_utf8_byte;
        fragment.continuation_before = continuation;
        fragment.continuation_after = metrics.reached_end == 0u;
        fragment.layout_fingerprint_high = metrics.fingerprint_high;
        fragment.layout_fingerprint_low = metrics.fingerprint_low;
        status = fl_emit(sink, &fragment, result, hash);
        if (status != FW_STATUS_OK) return status;
        *cursor_y = fl_max(*cursor_y,
            metrics.used_bounds.y + metrics.used_bounds.height);
        start = metrics.end_utf8_byte;
        if (metrics.reached_end != 0u) return FW_STATUS_OK;
        continuation = 1u;
    }
}

static fw_status fl_compose_object(const fw_flow_layout_request_v1 *request,
    const fw_flow_item_v1 *item, const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink, fw_rect_f32 content, float *cursor_y,
    uint32_t *ordinal, const fl_budget *budget,
    fw_flow_layout_result_v1 *result, fl_hash *hash) {
    const fw_child_measure_service_v1 *children = services->children;
    fw_child_measure_request_v1 measure_request;
    fw_child_measure_result_v1 measured;
    fw_flow_fragment_v1 fragment;
    fw_size_f32 size;
    char id_buffer[256];
    fw_status status;
    float ratio = 0.0f;
    if (result->fragment_count >= budget->fragments ||
        item->placement.min_width > content.width)
        return FW_STATUS_RESOURCE_LIMIT;
    if (children == NULL || children->struct_size < sizeof(*children) ||
        children->measure_child == NULL) return FW_STATUS_INVALID_ARGUMENT;
    memset(&measure_request, 0, sizeof(measure_request));
    measure_request.struct_size = sizeof(measure_request);
    measure_request.item_id = item->id;
    measure_request.content_id = item->content_id;
    measure_request.content_kind = item->content_kind;
    measure_request.constraints.struct_size =
        sizeof(measure_request.constraints);
    measure_request.constraints.min_width = item->placement.min_width;
    measure_request.constraints.max_width = item->placement.max_width == 0.0f ?
        content.width : fl_min(item->placement.max_width, content.width);
    measure_request.constraints.min_height = item->placement.min_height;
    measure_request.constraints.max_height = item->placement.max_height == 0.0f ?
        content.height : item->placement.max_height;
    measure_request.constraints.em_size = item->text_style.font_size;
    measure_request.constraints.line_height =
        item->text_style.font_size * item->text_style.line_height_multiplier;
    measure_request.target = request->target;
    memset(&measured, 0, sizeof(measured));
    measured.struct_size = sizeof(measured);
    status = children->measure_child(children->user_data, &measure_request,
        &measured);
    if (status != FW_STATUS_OK) return status;
    if (measured.struct_size < sizeof(measured) ||
        !fl_bool(measured.has_intrinsic_size) || !fl_bool(measured.splittable) ||
        !fl_bool(measured.used_fallback) ||
        !fl_nonnegative(measured.intrinsic_size.width) ||
        !fl_nonnegative(measured.intrinsic_size.height) ||
        !fl_nonnegative(measured.fallback_size.width) ||
        !fl_nonnegative(measured.fallback_size.height) ||
        !fl_bool(measured.aspect_ratio.has_value) ||
        (measured.aspect_ratio.has_value != 0u &&
            (!fl_finite(measured.aspect_ratio.value) ||
             measured.aspect_ratio.value <= 0.0f)))
        return FW_STATUS_INVALID_STATE;
    size = measured.has_intrinsic_size != 0u ? measured.intrinsic_size :
        measured.fallback_size;
    if (measured.aspect_ratio.has_value != 0u)
        ratio = measured.aspect_ratio.value;
    else if (size.height > 0.0f)
        ratio = size.width / size.height;
    if (item->placement.requested_width > 0.0f)
        size.width = item->placement.requested_width;
    if (item->placement.requested_height > 0.0f)
        size.height = item->placement.requested_height;
    if (item->placement.requested_width > 0.0f &&
        item->placement.requested_height == 0.0f && ratio > 0.0f)
        size.height = size.width / ratio;
    if (item->placement.requested_height > 0.0f &&
        item->placement.requested_width == 0.0f && ratio > 0.0f)
        size.width = size.height * ratio;
    size.width = fl_clamp_dimension(size.width, item->placement.min_width,
        item->placement.max_width);
    size.height = fl_clamp_dimension(size.height, item->placement.min_height,
        item->placement.max_height);
    if (size.width > content.width && item->placement.allow_scale_down != 0u) {
        const float scale = content.width / size.width;
        size.width = content.width;
        size.height *= scale;
    }
    if (size.width <= 0.0f || size.height <= 0.0f ||
        size.width < item->placement.min_width ||
        size.height < item->placement.min_height ||
        size.width > content.width ||
        *cursor_y + size.height > content.y + content.height)
        return FW_STATUS_RESOURCE_LIMIT;
    memset(&fragment, 0, sizeof(fragment));
    fragment.struct_size = sizeof(fragment);
    fragment.kind = measured.used_fallback != 0u ?
        FW_FLOW_FRAGMENT_PLACEHOLDER : FW_FLOW_FRAGMENT_OBJECT;
    status = fl_make_fragment_id(id_buffer, sizeof(id_buffer), item->id,
        request->layout_revision, (*ordinal)++, &fragment.derived_fragment_id);
    if (status != FW_STATUS_OK) return status;
    fragment.source_item_id = item->id;
    fragment.content_kind = item->content_kind;
    fragment.page_index = 0u;
    fragment.bounds.x = content.x + item->placement.offset_x;
    fragment.bounds.y = *cursor_y + item->placement.offset_y;
    fragment.bounds.width = size.width;
    fragment.bounds.height = size.height;
    fragment.clip = content;
    fragment.z = item->placement.z;
    fragment.layout_fingerprint_high = measured.fingerprint_high;
    fragment.layout_fingerprint_low = measured.fingerprint_low;
    status = fl_emit(sink, &fragment, result, hash);
    if (status != FW_STATUS_OK) return status;
    *cursor_y += size.height;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fl_validate(fw_plugin_handle plugin,
    const fw_flow_layout_request_v1 *request,
    fw_flow_validation_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = fl_validate_request(plugin, request, &key);
    out_result->status = status;
    out_result->diagnostic_key = fl_view(key);
    return status;
}

static fw_status FW_CALL fl_compose(fw_plugin_handle plugin,
    const fw_flow_layout_request_v1 *request,
    const fw_flow_layout_services_v1 *services,
    const fw_flow_plan_sink_v1 *sink,
    fw_flow_layout_result_v1 *out_result) {
    const char *key;
    fl_budget budget;
    fl_hash hash;
    fw_flow_page_v1 page;
    fw_rect_f32 content;
    float cursor_y;
    float previous_bottom = 0.0f;
    uint32_t ordinal = 0u;
    uint32_t size;
    uint32_t page_open = 0u;
    fw_status status;
    fw_status end_status;
    size_t i;
    char page_id[96];
    int page_id_length;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = fl_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    if (services == NULL || services->struct_size < sizeof(*services) ||
        sink == NULL || sink->struct_size < sizeof(*sink) ||
        sink->begin_page == NULL || sink->emit_fragment == NULL ||
        sink->end_page == NULL) return FW_STATUS_INVALID_ARGUMENT;
    if (request->page_template.mode != FW_FLOW_CONTINUOUS)
        return FW_STATUS_UNSUPPORTED;
    for (i = 0u; i < request->item_count; ++i) {
        if (request->items[i].placement.mode != FW_FLOW_PLACE_BLOCK &&
            request->items[i].placement.mode != FW_FLOW_PLACE_INLINE)
            return FW_STATUS_UNSUPPORTED;
        if (request->items[i].placement.mode == FW_FLOW_PLACE_INLINE ||
            request->items[i].kind != FW_FLOW_ITEM_PARAGRAPH) continue;
        {
            size_t j;
            for (j = 0u; j < request->items[i].segment_count; ++j) {
                if (request->items[i].segments[j].kind ==
                    FW_FLOW_SEGMENT_OBJECT) return FW_STATUS_UNSUPPORTED;
            }
        }
    }
    budget = fl_resolve_budget(&request->budget);
    if (budget.pages < 1u) return FW_STATUS_RESOURCE_LIMIT;
    hash = fl_request_hash(request);
    page_id_length = snprintf(page_id, sizeof(page_id),
        "vp:%016llx%016llx:%llu:0",
        (unsigned long long)hash.high, (unsigned long long)hash.low,
        (unsigned long long)request->layout_revision);
    if (page_id_length < 0 || (size_t)page_id_length >= sizeof(page_id))
        return FW_STATUS_RESOURCE_LIMIT;
    content.x = request->page_template.margins.left;
    content.y = request->page_template.margins.top;
    content.width = request->page_template.page_size.width -
        request->page_template.margins.left -
        request->page_template.margins.right;
    content.height = request->page_template.page_size.height -
        request->page_template.margins.top -
        request->page_template.margins.bottom;
    memset(&page, 0, sizeof(page));
    page.struct_size = sizeof(page);
    page.page_index = 0u;
    page.derived_page_id.data = page_id;
    page.derived_page_id.length = (size_t)page_id_length;
    page.size = request->page_template.page_size;
    page.content_bounds = content;
    page.column_count = 1u;
    status = sink->begin_page(sink->user_data, &page);
    if (status != FW_STATUS_OK) return FW_STATUS_SINK_REJECTED;
    page_open = 1u;
    out_result->page_count = 1u;
    cursor_y = content.y;
    for (i = 0u; i < request->item_count; ++i) {
        const fw_flow_item_v1 *item = &request->items[i];
        if (item->placement.mode == FW_FLOW_PLACE_INLINE) continue;
        cursor_y += fl_max(previous_bottom, item->placement.margins.top);
        if (item->kind == FW_FLOW_ITEM_PARAGRAPH)
            status = fl_compose_paragraph(request, item, services, sink,
                content, &cursor_y, &ordinal, &budget, out_result, &hash);
        else
            status = fl_compose_object(request, item, services, sink, content,
                &cursor_y, &ordinal, &budget, out_result, &hash);
        if (status != FW_STATUS_OK) break;
        previous_bottom = item->placement.margins.bottom;
    }
    if (status == FW_STATUS_OK) cursor_y += previous_bottom;
    end_status = page_open != 0u ? sink->end_page(sink->user_data, 0u) :
        FW_STATUS_OK;
    if (status == FW_STATUS_OK && end_status != FW_STATUS_OK)
        status = FW_STATUS_SINK_REJECTED;
    out_result->continuous_extent.width = request->page_template.page_size.width;
    out_result->continuous_extent.height = cursor_y +
        request->page_template.margins.bottom;
    out_result->plan_key_high = hash.high;
    out_result->plan_key_low = hash.low;
    out_result->complete = status == FW_STATUS_OK;
    return status;
}

static fw_status FW_CALL fl_get_parameter_schema(fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!fl_context_valid(plugin) || out_schema_json == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    out_schema_json->data = fl_parameter_schema;
    out_schema_json->length = sizeof(fl_parameter_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_flow_layout_api_v1 fl_layout_api = {
    sizeof(fw_flow_layout_api_v1), FW_FLOW_LAYOUT_INTERFACE_VERSION,
    fl_validate, fl_compose, fl_get_parameter_schema,
};

static const fw_plugin_descriptor_v1 *FW_CALL fl_get_descriptor(void) {
    return &fl_descriptor;
}

static fw_status FW_CALL fl_load(const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    fl_context *context;
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL || host->struct_size < sizeof(*host) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR)
        return FW_STATUS_INVALID_ARGUMENT;
    context = (fl_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FW_STATUS_OUT_OF_MEMORY;
    context->magic = FL_MAGIC;
    context->host = *host;
    *out_handle = context;
    return FW_STATUS_OK;
}

static void FW_CALL fl_unload(fw_plugin_handle handle) {
    fl_context *context = (fl_context *)handle;
    if (context != NULL && context->magic == FL_MAGIC) {
        context->magic = 0u;
        free(context);
    }
}

static fw_status FW_CALL fl_query_interface(fw_plugin_handle handle,
    fw_string_view interface_id, uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (!fl_context_valid(handle) || !fl_string_shape(interface_id))
        return FW_STATUS_INVALID_ARGUMENT;
    if (minimum_version > FW_FLOW_LAYOUT_INTERFACE_VERSION ||
        !fl_string_literal(interface_id, FW_FLOW_LAYOUT_INTERFACE_ID))
        return FW_STATUS_NOT_FOUND;
    *out_interface = &fl_layout_api;
    return FW_STATUS_OK;
}

static const fw_plugin_api_v1 fl_plugin_api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    fl_get_descriptor, fl_load, fl_unload, fl_query_interface,
};

FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_flow_layout_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor > FW_ABI_VERSION_MINOR) return NULL;
    return &fl_plugin_api;
}

#if defined(FACETWIRE_FLOW_LAYOUT_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    return facetwire_flow_layout_plugin_query(requested_abi);
}
#endif
