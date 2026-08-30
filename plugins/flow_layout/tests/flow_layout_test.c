/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/child_measure_service.h>
#include <facetwire/flow_layout.h>
#include <facetwire/text_fragment_service.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #expression); \
        return 0; \
    } \
} while (0)

typedef struct fake_text_state {
    uint32_t calls;
    uint32_t zero_progress;
    size_t max_bytes_per_fragment;
    float minimum_region_height;
    size_t max_inline_parts_per_fragment;
    uint32_t disable_inline_parts;
    uint32_t invalid_inline_part;
    uint32_t legacy_block_abi;
} fake_text_state;

typedef struct fake_child_state {
    uint32_t calls;
    uint32_t fallback;
    uint32_t zero_size;
} fake_child_state;

typedef struct fake_sink_state {
    uint32_t begin_count;
    uint32_t end_count;
    uint32_t fragment_count;
    uint32_t reject_fragment;
    fw_flow_fragment_kind kinds[16];
    char source_ids[16][32];
    char page_ids[16][96];
    uint32_t page_indices[16];
    uint32_t column_indices[16];
    uint32_t page_column_counts[16];
    size_t text_starts[16];
    size_t text_ends[16];
    uint32_t continuation_before[16];
    uint32_t continuation_after[16];
    fw_rect_f32 bounds[16];
} fake_sink_state;

typedef struct flow_fixture {
    fw_flow_segment_v1 first_segments[3];
    fw_flow_segment_v1 last_segments[1];
    fw_flow_item_v1 items[3];
    fw_flow_layout_request_v1 request;
} flow_fixture;

static fw_string_view view_of(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static size_t paragraph_bytes(const fw_text_fragment_request_v1 *request) {
    size_t result = 0u;
    size_t i;
    for (i = 0u; i < request->segment_count; ++i) {
        if (request->segments[i].kind == FW_FLOW_SEGMENT_TEXT)
            result += request->segments[i].text.length;
    }
    return result;
}

static void include_bounds(fw_rect_f32 *used, uint32_t *has_bounds,
    fw_rect_f32 value) {
    if (*has_bounds == 0u) {
        *used = value;
        *has_bounds = 1u;
    } else {
        const float right = fmaxf(used->x + used->width,
            value.x + value.width);
        const float bottom = fmaxf(used->y + used->height,
            value.y + value.height);
        used->x = fminf(used->x, value.x);
        used->y = fminf(used->y, value.y);
        used->width = right - used->x;
        used->height = bottom - used->y;
    }
}

static fw_status fake_measure_inline_text(fake_text_state *state,
    const fw_text_fragment_request_v1 *request,
    fw_text_fragment_metrics_v1 *out_metrics, size_t total) {
    size_t text_cursor = request->start_utf8_byte;
    size_t object_cursor = request->start_inline_object_index;
    size_t part_count = 0u;
    const size_t part_limit = state->max_inline_parts_per_fragment == 0u ?
        request->part_capacity : state->max_inline_parts_per_fragment;
    const float line_height = 24.0f;
    const float baseline = 18.0f;
    const float left = request->region.x;
    const float right = request->region.x + request->region.width;
    float cursor = request->direction == FW_TEXT_DIRECTION_RTL ? right : left;
    uint32_t has_bounds = 0u;
    if (request->parts == NULL || request->part_capacity == 0u ||
        request->start_inline_object_index > request->inline_object_count ||
        request->region.height < line_height)
        return FW_STATUS_INVALID_ARGUMENT;
    while ((text_cursor < total ||
            object_cursor < request->inline_object_count) &&
           part_count < request->part_capacity && part_count < part_limit) {
        const size_t next_object_offset =
            object_cursor < request->inline_object_count ?
            request->inline_objects[object_cursor].text_offset_utf8_byte :
            total;
        fw_text_fragment_part_v1 *part = &request->parts[part_count];
        if (text_cursor < next_object_offset) {
            const float width = (float)(next_object_offset - text_cursor) *
                8.0f;
            if ((request->direction == FW_TEXT_DIRECTION_RTL &&
                 cursor - width < left) ||
                (request->direction != FW_TEXT_DIRECTION_RTL &&
                 cursor + width > right))
                return FW_STATUS_RESOURCE_LIMIT;
            part->kind = FW_TEXT_FRAGMENT_PART_TEXT;
            part->text_start_utf8_byte = text_cursor;
            part->text_end_utf8_byte = next_object_offset;
            part->bounds.x = request->direction == FW_TEXT_DIRECTION_RTL ?
                cursor - width : cursor;
            part->bounds.y = request->region.y;
            part->bounds.width = width;
            part->bounds.height = line_height;
            cursor += request->direction == FW_TEXT_DIRECTION_RTL ?
                -width : width;
            text_cursor = next_object_offset;
        } else {
            const fw_text_inline_object_v1 *object =
                &request->inline_objects[object_cursor];
            const float occupied = object->margins.left + object->size.width +
                object->margins.right;
            float object_y;
            if ((request->direction == FW_TEXT_DIRECTION_RTL &&
                 cursor - occupied < left) ||
                (request->direction != FW_TEXT_DIRECTION_RTL &&
                 cursor + occupied > right))
                return FW_STATUS_RESOURCE_LIMIT;
            part->kind = FW_TEXT_FRAGMENT_PART_OBJECT;
            part->inline_object_index = object_cursor;
            part->bounds.x = request->direction == FW_TEXT_DIRECTION_RTL ?
                cursor - object->margins.right - object->size.width :
                cursor + object->margins.left;
            if (object->baseline_mode == FW_FLOW_BASELINE_BASELINE)
                object_y = request->region.y + baseline - object->size.height;
            else if (object->baseline_mode == FW_FLOW_BASELINE_MIDDLE)
                object_y = request->region.y +
                    (line_height - object->size.height) * 0.5f;
            else if (object->baseline_mode == FW_FLOW_BASELINE_TEXT_BOTTOM)
                object_y = request->region.y + line_height -
                    object->size.height;
            else
                object_y = request->region.y;
            part->bounds.y = object_y + object->offset_y;
            part->bounds.x += object->offset_x;
            part->bounds.width = object->size.width;
            part->bounds.height = object->size.height;
            cursor += request->direction == FW_TEXT_DIRECTION_RTL ?
                -occupied : occupied;
            ++object_cursor;
        }
        include_bounds(&out_metrics->used_bounds, &has_bounds, part->bounds);
        ++part_count;
    }
    if (state->invalid_inline_part != 0u && part_count != 0u)
        request->parts[0].text_start_utf8_byte += 1u;
    out_metrics->end_utf8_byte = text_cursor;
    out_metrics->end_inline_object_index = object_cursor;
    out_metrics->part_count = part_count;
    out_metrics->line_count = 1u;
    out_metrics->reached_end = text_cursor == total &&
        object_cursor == request->inline_object_count;
    out_metrics->fingerprint_high = UINT64_C(0x1111222233334444);
    out_metrics->fingerprint_low = (uint64_t)total ^ (uint64_t)object_cursor;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_measure_text(void *user_data,
    const fw_text_fragment_request_v1 *request,
    fw_text_fragment_metrics_v1 *out_metrics) {
    fake_text_state *state = (fake_text_state *)user_data;
    const size_t total = paragraph_bytes(request);
    size_t end = total;
    ++state->calls;
    if (out_metrics == NULL || out_metrics->struct_size < sizeof(*out_metrics) ||
        request->start_utf8_byte > total)
        return FW_STATUS_INVALID_ARGUMENT;
    if (request->inline_object_count != 0u)
        return fake_measure_inline_text(state, request, out_metrics, total);
    if (request->region.height < (state->minimum_region_height > 0.0f ?
        state->minimum_region_height : 24.0f)) {
        return state->minimum_region_height > 0.0f ?
            FW_STATUS_RESOURCE_LIMIT : FW_STATUS_INVALID_ARGUMENT;
    }
    if (state->max_bytes_per_fragment != 0u &&
        request->start_utf8_byte + state->max_bytes_per_fragment < total)
        end = request->start_utf8_byte + state->max_bytes_per_fragment;
    out_metrics->end_utf8_byte = state->zero_progress != 0u ?
        request->start_utf8_byte : end;
    out_metrics->used_bounds.x = request->region.x;
    out_metrics->used_bounds.y = request->region.y;
    out_metrics->used_bounds.width = request->region.width;
    out_metrics->used_bounds.height = 24.0f;
    out_metrics->line_count = 1u;
    out_metrics->reached_end = state->zero_progress == 0u && end == total;
    out_metrics->fingerprint_high = UINT64_C(0x1111222233334444);
    out_metrics->fingerprint_low = (uint64_t)total;
    if (state->legacy_block_abi != 0u)
        out_metrics->struct_size = offsetof(fw_text_fragment_metrics_v1,
            end_inline_object_index);
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_draw_text(void *user_data,
    const fw_text_fragment_request_v1 *request,
    const fw_text_fragment_metrics_v1 *expected,
    const fw_display_list_sink_v1 *display_list) {
    (void)user_data;
    (void)request;
    (void)expected;
    (void)display_list;
    return FW_STATUS_UNSUPPORTED;
}

static fw_status FW_CALL fake_measure_child(void *user_data,
    const fw_child_measure_request_v1 *request,
    fw_child_measure_result_v1 *out_result) {
    fake_child_state *state = (fake_child_state *)user_data;
    (void)request;
    ++state->calls;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    out_result->intrinsic_size.width = 120.0f;
    out_result->intrinsic_size.height = 80.0f;
    out_result->fallback_size.width = 96.0f;
    out_result->fallback_size.height = 64.0f;
    out_result->aspect_ratio.has_value = 1u;
    out_result->aspect_ratio.value = 1.5f;
    out_result->has_intrinsic_size = state->fallback == 0u;
    out_result->used_fallback = state->fallback;
    out_result->fingerprint_high = UINT64_C(0x5555666677778888);
    out_result->fingerprint_low = UINT64_C(0x9999aaaabbbbcccc);
    if (state->zero_size != 0u) {
        out_result->intrinsic_size.width = 0.0f;
        out_result->intrinsic_size.height = 0.0f;
    }
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_begin_page(void *user_data,
    const fw_flow_page_v1 *page) {
    fake_sink_state *state = (fake_sink_state *)user_data;
    size_t copy_length;
    if (page == NULL || page->page_index != state->begin_count ||
        page->column_count == 0u || state->begin_count >= 16u)
        return FW_STATUS_PLUGIN_ERROR;
    state->page_column_counts[state->begin_count] = page->column_count;
    copy_length = page->derived_page_id.length;
    if (copy_length >= sizeof(state->page_ids[state->begin_count]))
        copy_length = sizeof(state->page_ids[state->begin_count]) - 1u;
    memcpy(state->page_ids[state->begin_count],
        page->derived_page_id.data, copy_length);
    state->page_ids[state->begin_count][copy_length] = '\0';
    ++state->begin_count;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_emit_fragment(void *user_data,
    const fw_flow_fragment_v1 *fragment) {
    fake_sink_state *state = (fake_sink_state *)user_data;
    size_t copy_length;
    const uint32_t index = state->fragment_count;
    if (state->reject_fragment != 0u &&
        index + 1u == state->reject_fragment) return FW_STATUS_PLUGIN_ERROR;
    if (index >= 16u || fragment == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    state->kinds[index] = fragment->kind;
    state->bounds[index] = fragment->bounds;
    state->page_indices[index] = fragment->page_index;
    state->column_indices[index] = fragment->column_index;
    state->text_starts[index] = fragment->text_start_utf8_byte;
    state->text_ends[index] = fragment->text_end_utf8_byte;
    state->continuation_before[index] = fragment->continuation_before;
    state->continuation_after[index] = fragment->continuation_after;
    copy_length = fragment->source_item_id.length;
    if (copy_length >= sizeof(state->source_ids[index]))
        copy_length = sizeof(state->source_ids[index]) - 1u;
    memcpy(state->source_ids[index], fragment->source_item_id.data,
        copy_length);
    state->source_ids[index][copy_length] = '\0';
    ++state->fragment_count;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_end_page(void *user_data, uint32_t page_index) {
    fake_sink_state *state = (fake_sink_state *)user_data;
    if (page_index != state->end_count) return FW_STATUS_PLUGIN_ERROR;
    ++state->end_count;
    return FW_STATUS_OK;
}

static fw_flow_placement_v1 block_placement(float top, float bottom) {
    fw_flow_placement_v1 value;
    memset(&value, 0, sizeof(value));
    value.struct_size = sizeof(value);
    value.mode = FW_FLOW_PLACE_BLOCK;
    value.margins.top = top;
    value.margins.bottom = bottom;
    value.max_width = 10000.0f;
    value.max_height = 10000.0f;
    value.allow_scale_down = 1u;
    return value;
}

static fw_flow_break_policy_v1 break_policy(void) {
    fw_flow_break_policy_v1 value;
    memset(&value, 0, sizeof(value));
    value.struct_size = sizeof(value);
    value.orphans = 2u;
    value.widows = 2u;
    return value;
}

static fw_text_style_v1 text_style(void) {
    fw_text_style_v1 value;
    memset(&value, 0, sizeof(value));
    value.struct_size = sizeof(value);
    value.font_size = 16.0f;
    value.font_weight = 400u;
    value.line_height_multiplier = 1.5f;
    value.color.alpha = 1.0f;
    return value;
}

static void make_fixture(flow_fixture *fixture, const char *flow_id,
    const char *first_id, const char *object_id, const char *last_id,
    const char *first_text, const char *last_text) {
    fw_flow_item_v1 *item;
    memset(fixture, 0, sizeof(*fixture));
    fixture->first_segments[0].struct_size = sizeof(fixture->first_segments[0]);
    fixture->first_segments[0].kind = FW_FLOW_SEGMENT_TEXT;
    fixture->first_segments[0].text = view_of(first_text);
    fixture->last_segments[0].struct_size = sizeof(fixture->last_segments[0]);
    fixture->last_segments[0].kind = FW_FLOW_SEGMENT_TEXT;
    fixture->last_segments[0].text = view_of(last_text);
    item = &fixture->items[0];
    item->struct_size = sizeof(*item);
    item->id = view_of(first_id);
    item->kind = FW_FLOW_ITEM_PARAGRAPH;
    item->segments = fixture->first_segments;
    item->segment_count = 1u;
    item->text_style = text_style();
    item->direction = FW_TEXT_DIRECTION_LTR;
    item->placement = block_placement(4.0f, 8.0f);
    item->break_policy = break_policy();
    item = &fixture->items[1];
    item->struct_size = sizeof(*item);
    item->id = view_of(object_id);
    item->kind = FW_FLOW_ITEM_OBJECT;
    item->content_id = view_of("asset:hero");
    item->content_kind = view_of("image");
    item->placement = block_placement(12.0f, 6.0f);
    item->break_policy = break_policy();
    item = &fixture->items[2];
    item->struct_size = sizeof(*item);
    item->id = view_of(last_id);
    item->kind = FW_FLOW_ITEM_PARAGRAPH;
    item->segments = fixture->last_segments;
    item->segment_count = 1u;
    item->text_style = text_style();
    item->direction = FW_TEXT_DIRECTION_LTR;
    item->placement = block_placement(2.0f, 3.0f);
    item->break_policy = break_policy();
    fixture->request.struct_size = sizeof(fixture->request);
    fixture->request.request_id = 7u;
    fixture->request.flow_id = view_of(flow_id);
    fixture->request.items = fixture->items;
    fixture->request.item_count = 3u;
    fixture->request.page_template.struct_size = sizeof(fixture->request.page_template);
    fixture->request.page_template.mode = FW_FLOW_CONTINUOUS;
    fixture->request.page_template.page_size.width = 600.0f;
    fixture->request.page_template.page_size.height = 800.0f;
    fixture->request.page_template.margins.left = 20.0f;
    fixture->request.page_template.margins.top = 20.0f;
    fixture->request.page_template.margins.right = 20.0f;
    fixture->request.page_template.margins.bottom = 20.0f;
    fixture->request.page_template.column_count = 1u;
    fixture->request.page_template.minimum_text_width = 80.0f;
    fixture->request.budget.struct_size = sizeof(fixture->request.budget);
    fixture->request.target.struct_size = sizeof(fixture->request.target);
    fixture->request.target.device_pixel_ratio = 1.0f;
    fixture->request.target.font_scale = 1.0f;
    fixture->request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    fixture->request.target.supports_alpha = 1u;
    fixture->request.document_revision = 3u;
    fixture->request.layout_revision = 5u;
    fixture->request.profile_key = view_of("desktop-600");
}

static void make_inline_fixture(flow_fixture *fixture,
    fw_flow_baseline_mode baseline_mode) {
    make_fixture(fixture, "inline", "paragraph.inline", "badge.inline",
        "unused", "Hi", "unused");
    fixture->first_segments[0].text = view_of("Hi");
    fixture->first_segments[1].struct_size =
        sizeof(fixture->first_segments[1]);
    fixture->first_segments[1].kind = FW_FLOW_SEGMENT_OBJECT;
    fixture->first_segments[1].object_item_id =
        fixture->items[1].id;
    fixture->first_segments[1].baseline_mode = baseline_mode;
    fixture->first_segments[2].struct_size =
        sizeof(fixture->first_segments[2]);
    fixture->first_segments[2].kind = FW_FLOW_SEGMENT_TEXT;
    fixture->first_segments[2].text = view_of("OK");
    fixture->items[0].segments = fixture->first_segments;
    fixture->items[0].segment_count = 3u;
    fixture->items[1].placement.mode = FW_FLOW_PLACE_INLINE;
    fixture->items[1].placement.requested_width = 24.0f;
    fixture->items[1].placement.requested_height = 16.0f;
    fixture->items[1].placement.margins.left = 1.0f;
    fixture->items[1].placement.margins.top = 0.0f;
    fixture->items[1].placement.margins.right = 2.0f;
    fixture->items[1].placement.margins.bottom = 0.0f;
    fixture->request.item_count = 2u;
}

static fw_flow_layout_services_v1 make_services(fake_text_state *text_state,
    fake_child_state *child_state, fw_text_fragment_service_v1 *text,
    fw_child_measure_service_v1 *children) {
    fw_flow_layout_services_v1 services;
    memset(text, 0, sizeof(*text));
    text->struct_size = text_state->legacy_block_abi != 0u ?
        offsetof(fw_text_fragment_service_v1, flags) : sizeof(*text);
    text->user_data = text_state;
    text->measure_next = fake_measure_text;
    text->draw_exact = fake_draw_text;
    text->flags = text_state->disable_inline_parts == 0u ?
        FW_TEXT_FRAGMENT_SERVICE_INLINE_PARTS : 0u;
    memset(children, 0, sizeof(*children));
    children->struct_size = sizeof(*children);
    children->user_data = child_state;
    children->measure_child = fake_measure_child;
    memset(&services, 0, sizeof(services));
    services.struct_size = sizeof(services);
    services.text = text;
    services.children = children;
    return services;
}

static fw_flow_plan_sink_v1 make_sink(fake_sink_state *state) {
    fw_flow_plan_sink_v1 sink;
    memset(&sink, 0, sizeof(sink));
    sink.struct_size = sizeof(sink);
    sink.user_data = state;
    sink.begin_page = fake_begin_page;
    sink.emit_fragment = fake_emit_fragment;
    sink.end_page = fake_end_page;
    return sink;
}

static int compose_fixture(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle, flow_fixture *fixture, fake_text_state *text_state,
    fake_child_state *child_state, fake_sink_state *sink_state,
    fw_flow_layout_result_v1 *result) {
    fw_text_fragment_service_v1 text;
    fw_child_measure_service_v1 children;
    fw_flow_layout_services_v1 services = make_services(text_state,
        child_state, &text, &children);
    fw_flow_plan_sink_v1 sink = make_sink(sink_state);
    result->struct_size = sizeof(*result);
    return api->compose(handle, &fixture->request, &services, &sink, result);
}

static int test_descriptor_and_validation(const fw_plugin_api_v1 *plugin,
    fw_plugin_handle handle, const fw_flow_layout_api_v1 *api) {
    const fw_plugin_descriptor_v1 *descriptor = plugin->get_descriptor();
    fw_flow_validation_result_v1 result;
    fw_string_view schema = {0};
    flow_fixture fixture;
    CHECK(descriptor != NULL && descriptor->capability_count == 1u);
    CHECK(api->get_parameter_schema(handle, &schema) == FW_STATUS_OK);
    CHECK(schema.data != NULL && strstr(schema.data, "continuous") != NULL);
    CHECK(strstr(schema.data, "virtual-pages") != NULL);
    CHECK(strstr(schema.data, "columns") != NULL);
    make_fixture(&fixture, "demo", "p1", "hero", "p2", "Hello", "World");
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    CHECK(api->validate(handle, &fixture.request, &result) == FW_STATUS_OK);
    fixture.items[1].id = fixture.items[0].id;
    result.struct_size = sizeof(result);
    CHECK(api->validate(handle, &fixture.request, &result) == FW_STATUS_INVALID_ARGUMENT);
    CHECK(result.status == FW_STATUS_INVALID_ARGUMENT);
    return 1;
}

static int test_continuous_block_order(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "demo", "p1", "hero", "p2", "Hello", "World");
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 1u);
    CHECK(result.fragment_count == 3u && result.text_fragment_count == 2u);
    CHECK(result.object_fragment_count == 1u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    CHECK(strcmp(sink_state.source_ids[0], "p1") == 0);
    CHECK(strcmp(sink_state.source_ids[1], "hero") == 0);
    CHECK(strcmp(sink_state.source_ids[2], "p2") == 0);
    CHECK(sink_state.kinds[0] == FW_FLOW_FRAGMENT_TEXT);
    CHECK(sink_state.kinds[1] == FW_FLOW_FRAGMENT_OBJECT);
    CHECK(sink_state.kinds[2] == FW_FLOW_FRAGMENT_TEXT);
    CHECK(fabsf(sink_state.bounds[0].y - 24.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[1].y - 60.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[2].y - 146.0f) < 0.001f);
    CHECK(text_state.calls == 2u && child_state.calls == 1u);
    return 1;
}

static int test_deterministic_key(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture first, second;
    char flow[] = "demo", first_id[] = "p1", object_id[] = "hero";
    char last_id[] = "p2", first_text[] = "Hello", last_text[] = "World";
    fake_text_state text_a = {0}, text_b = {0};
    fake_child_state child_a = {0}, child_b = {0};
    fake_sink_state sink_a = {0}, sink_b = {0};
    fw_flow_layout_result_v1 result_a = {0}, result_b = {0};
    make_fixture(&first, "demo", "p1", "hero", "p2", "Hello", "World");
    make_fixture(&second, flow, first_id, object_id, last_id, first_text, last_text);
    CHECK(compose_fixture(api, handle, &first, &text_a, &child_a, &sink_a,
        &result_a) == FW_STATUS_OK);
    CHECK(compose_fixture(api, handle, &second, &text_b, &child_b, &sink_b,
        &result_b) == FW_STATUS_OK);
    CHECK(result_a.plan_key_high == result_b.plan_key_high);
    CHECK(result_a.plan_key_low == result_b.plan_key_low);
    return 1;
}

static int test_long_id_and_fallback(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    char long_id[512];
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    memset(long_id, 'a', sizeof(long_id) - 1u);
    long_id[sizeof(long_id) - 1u] = '\0';
    make_fixture(&fixture, "demo", long_id, "hero", "p2", "Hello", "World");
    child_state.fallback = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u);
    CHECK(sink_state.kinds[1] == FW_FLOW_FRAGMENT_PLACEHOLDER);
    return 1;
}

static int test_failures_are_balanced(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "demo", "p1", "hero", "p2", "Hello", "World");
    sink_state.reject_fragment = 2u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_SINK_REJECTED);
    CHECK(result.complete == 0u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    memset(&sink_state, 0, sizeof(sink_state));
    memset(&result, 0, sizeof(result));
    text_state.zero_progress = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_INVALID_STATE);
    CHECK(result.complete == 0u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    return 1;
}

static int test_virtual_pages_block_pagination(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "demo", "p1", "hero", "p2", "Hello", "World");
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    fixture.request.page_template.page_size.height = 140.0f;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 3u);
    CHECK(result.fragment_count == 3u);
    CHECK(sink_state.begin_count == 3u && sink_state.end_count == 3u);
    CHECK(sink_state.page_indices[0] == 0u);
    CHECK(sink_state.page_indices[1] == 1u);
    CHECK(sink_state.page_indices[2] == 2u);
    CHECK(fabsf(sink_state.bounds[2].y - 22.0f) < 0.001f);
    CHECK(strcmp(sink_state.page_ids[0], sink_state.page_ids[1]) != 0);
    CHECK(strcmp(sink_state.page_ids[1], sink_state.page_ids[2]) != 0);
    CHECK(fabsf(result.continuous_extent.height - 420.0f) < 0.001f);
    return 1;
}

static int test_paragraph_ranges_cross_pages(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "split", "p1", "hero", "p2", "abcdef", "unused");
    fixture.request.item_count = 1u;
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    text_state.max_bytes_per_fragment = 2u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 3u);
    CHECK(result.text_fragment_count == 3u);
    CHECK(sink_state.text_starts[0] == 0u &&
        sink_state.text_ends[0] == 2u);
    CHECK(sink_state.text_starts[1] == 2u &&
        sink_state.text_ends[1] == 4u);
    CHECK(sink_state.text_starts[2] == 4u &&
        sink_state.text_ends[2] == 6u);
    CHECK(sink_state.continuation_before[0] == 0u &&
        sink_state.continuation_after[0] == 1u);
    CHECK(sink_state.continuation_before[1] == 1u &&
        sink_state.continuation_after[1] == 1u);
    CHECK(sink_state.continuation_before[2] == 1u &&
        sink_state.continuation_after[2] == 0u);
    return 1;
}

static int test_virtual_page_budget_is_balanced(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "split", "p1", "hero", "p2", "abcdef", "unused");
    fixture.request.item_count = 1u;
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    fixture.request.budget.max_pages = 2u;
    text_state.max_bytes_per_fragment = 2u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_RESOURCE_LIMIT);
    CHECK(result.complete == 0u && result.page_count == 2u);
    CHECK(sink_state.begin_count == 2u && sink_state.end_count == 2u);
    return 1;
}

static int test_virtual_page_retries_text_on_fresh_page(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "retry", "p1", "hero", "p2", "Hello", "World");
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    fixture.request.page_template.page_size.height = 200.0f;
    text_state.minimum_region_height = 80.0f;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 2u);
    CHECK(sink_state.page_indices[2] == 1u);
    CHECK(fabsf(sink_state.bounds[2].y - 22.0f) < 0.001f);
    return 1;
}

static int test_virtual_page_rejects_zero_sized_object(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "zero", "p1", "hero", "p2", "Hello", "World");
    fixture.request.items = &fixture.items[1];
    fixture.request.item_count = 1u;
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    child_state.zero_size = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_RESOURCE_LIMIT);
    CHECK(result.complete == 0u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    CHECK(sink_state.fragment_count == 0u);
    return 1;
}

static int test_columns_block_progression(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "demo", "p1", "hero", "p2", "Hello", "World");
    fixture.request.page_template.mode = FW_FLOW_COLUMNS;
    fixture.request.page_template.column_count = 2u;
    fixture.request.page_template.column_gap = 20.0f;
    fixture.request.page_template.page_size.height = 160.0f;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 1u);
    CHECK(result.fragment_count == 3u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    CHECK(sink_state.page_column_counts[0] == 2u);
    CHECK(sink_state.page_indices[0] == 0u &&
        sink_state.page_indices[1] == 0u &&
        sink_state.page_indices[2] == 0u);
    CHECK(sink_state.column_indices[0] == 0u &&
        sink_state.column_indices[1] == 0u &&
        sink_state.column_indices[2] == 1u);
    CHECK(fabsf(sink_state.bounds[0].x - 20.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[2].x - 310.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[2].y - 22.0f) < 0.001f);
    return 1;
}

static int test_paragraph_ranges_cross_columns_and_pages(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "columns", "p1", "hero", "p2", "abcdef", "unused");
    fixture.request.item_count = 1u;
    fixture.request.page_template.mode = FW_FLOW_COLUMNS;
    fixture.request.page_template.column_count = 2u;
    fixture.request.page_template.column_gap = 20.0f;
    text_state.max_bytes_per_fragment = 2u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 2u);
    CHECK(result.text_fragment_count == 3u);
    CHECK(sink_state.page_column_counts[0] == 2u &&
        sink_state.page_column_counts[1] == 2u);
    CHECK(sink_state.page_indices[0] == 0u &&
        sink_state.page_indices[1] == 0u &&
        sink_state.page_indices[2] == 1u);
    CHECK(sink_state.column_indices[0] == 0u &&
        sink_state.column_indices[1] == 1u &&
        sink_state.column_indices[2] == 0u);
    CHECK(sink_state.text_starts[0] == 0u &&
        sink_state.text_ends[0] == 2u);
    CHECK(sink_state.text_starts[1] == 2u &&
        sink_state.text_ends[1] == 4u);
    CHECK(sink_state.text_starts[2] == 4u &&
        sink_state.text_ends[2] == 6u);
    return 1;
}

static int test_columns_page_budget_is_balanced(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "columns", "p1", "hero", "p2", "abcdef", "unused");
    fixture.request.item_count = 1u;
    fixture.request.page_template.mode = FW_FLOW_COLUMNS;
    fixture.request.page_template.column_count = 2u;
    fixture.request.page_template.column_gap = 20.0f;
    fixture.request.budget.max_pages = 1u;
    text_state.max_bytes_per_fragment = 2u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_RESOURCE_LIMIT);
    CHECK(result.complete == 0u && result.page_count == 1u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    CHECK(sink_state.fragment_count == 2u);
    return 1;
}

static int test_invalid_column_geometry(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    fw_flow_validation_result_v1 result = {0};
    make_fixture(&fixture, "columns", "p1", "hero", "p2", "Hello", "World");
    fixture.request.page_template.mode = FW_FLOW_COLUMNS;
    fixture.request.page_template.column_count = 2u;
    fixture.request.page_template.column_gap = 500.0f;
    result.struct_size = sizeof(result);
    CHECK(api->validate(handle, &fixture.request, &result) ==
        FW_STATUS_INVALID_ARGUMENT);
    CHECK(result.diagnostic_key.length == strlen("flow.invalid_column_geometry"));
    CHECK(memcmp(result.diagnostic_key.data, "flow.invalid_column_geometry",
        result.diagnostic_key.length) == 0);
    return 1;
}

static int test_inline_object_continuous_order(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_inline_fixture(&fixture, FW_FLOW_BASELINE_BASELINE);
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.page_count == 1u);
    CHECK(result.fragment_count == 3u && result.text_fragment_count == 2u);
    CHECK(result.object_fragment_count == 1u);
    CHECK(sink_state.kinds[0] == FW_FLOW_FRAGMENT_TEXT &&
        sink_state.kinds[1] == FW_FLOW_FRAGMENT_OBJECT &&
        sink_state.kinds[2] == FW_FLOW_FRAGMENT_TEXT);
    CHECK(strcmp(sink_state.source_ids[0], "paragraph.inline") == 0);
    CHECK(strcmp(sink_state.source_ids[1], "badge.inline") == 0);
    CHECK(strcmp(sink_state.source_ids[2], "paragraph.inline") == 0);
    CHECK(sink_state.text_starts[0] == 0u &&
        sink_state.text_ends[0] == 2u);
    CHECK(sink_state.text_starts[2] == 2u &&
        sink_state.text_ends[2] == 4u);
    CHECK(fabsf(sink_state.bounds[0].x - 20.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[1].x - 37.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[1].y - 26.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[2].x - 63.0f) < 0.001f);
    CHECK(text_state.calls == 1u && child_state.calls == 1u);
    return 1;
}

static int test_legacy_block_text_service_abi(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "legacy-block", "paragraph.first", "image.hero",
        "paragraph.last", "first", "last");
    text_state.legacy_block_abi = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.fragment_count == 3u);
    memset(&text_state, 0, sizeof(text_state));
    memset(&child_state, 0, sizeof(child_state));
    memset(&sink_state, 0, sizeof(sink_state));
    memset(&result, 0, sizeof(result));
    text_state.legacy_block_abi = 1u;
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.complete == 1u && result.fragment_count == 3u);
    return 1;
}

static int test_inline_baseline_modes(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    const fw_flow_baseline_mode modes[4] = {
        FW_FLOW_BASELINE_BASELINE, FW_FLOW_BASELINE_MIDDLE,
        FW_FLOW_BASELINE_TEXT_TOP, FW_FLOW_BASELINE_TEXT_BOTTOM};
    const float expected_y[4] = {26.0f, 28.0f, 24.0f, 32.0f};
    size_t index;
    for (index = 0u; index < 4u; ++index) {
        flow_fixture fixture;
        fake_text_state text_state = {0};
        fake_child_state child_state = {0};
        fake_sink_state sink_state = {0};
        fw_flow_layout_result_v1 result = {0};
        make_inline_fixture(&fixture, modes[index]);
        CHECK(compose_fixture(api, handle, &fixture, &text_state,
            &child_state, &sink_state, &result) == FW_STATUS_OK);
        CHECK(fabsf(sink_state.bounds[1].y - expected_y[index]) < 0.001f);
    }
    return 1;
}

static int test_inline_rtl_geometry(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_inline_fixture(&fixture, FW_FLOW_BASELINE_MIDDLE);
    fixture.items[0].direction = FW_TEXT_DIRECTION_RTL;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(fabsf(sink_state.bounds[0].x - 564.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[1].x - 538.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[2].x - 521.0f) < 0.001f);
    CHECK(strcmp(sink_state.source_ids[0], "paragraph.inline") == 0 &&
        strcmp(sink_state.source_ids[1], "badge.inline") == 0 &&
        strcmp(sink_state.source_ids[2], "paragraph.inline") == 0);
    return 1;
}

static int test_inline_parts_cross_pages_and_columns(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_inline_fixture(&fixture, FW_FLOW_BASELINE_BASELINE);
    fixture.request.page_template.mode = FW_FLOW_VIRTUAL_PAGES;
    fixture.request.page_template.page_size.height = 100.0f;
    text_state.max_inline_parts_per_fragment = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.page_count == 3u && result.fragment_count == 3u);
    CHECK(sink_state.page_indices[0] == 0u &&
        sink_state.page_indices[1] == 1u &&
        sink_state.page_indices[2] == 2u);
    CHECK(sink_state.kinds[1] == FW_FLOW_FRAGMENT_OBJECT);
    memset(&text_state, 0, sizeof(text_state));
    memset(&child_state, 0, sizeof(child_state));
    memset(&sink_state, 0, sizeof(sink_state));
    memset(&result, 0, sizeof(result));
    fixture.request.page_template.mode = FW_FLOW_COLUMNS;
    fixture.request.page_template.column_count = 2u;
    fixture.request.page_template.column_gap = 20.0f;
    text_state.max_inline_parts_per_fragment = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(result.page_count == 2u && result.fragment_count == 3u);
    CHECK(sink_state.page_indices[0] == 0u &&
        sink_state.page_indices[1] == 0u &&
        sink_state.page_indices[2] == 1u);
    CHECK(sink_state.column_indices[0] == 0u &&
        sink_state.column_indices[1] == 1u &&
        sink_state.column_indices[2] == 0u);
    return 1;
}

static int test_inline_capability_and_part_validation(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_inline_fixture(&fixture, FW_FLOW_BASELINE_BASELINE);
    text_state.disable_inline_parts = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_UNSUPPORTED);
    CHECK(sink_state.begin_count == 0u && sink_state.end_count == 0u);
    memset(&text_state, 0, sizeof(text_state));
    memset(&sink_state, 0, sizeof(sink_state));
    memset(&result, 0, sizeof(result));
    text_state.invalid_inline_part = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_INVALID_STATE);
    CHECK(result.complete == 0u);
    CHECK(sink_state.begin_count == 1u && sink_state.end_count == 1u);
    return 1;
}

static int test_inline_fallback_preserves_slot(
    const fw_flow_layout_api_v1 *api, fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_inline_fixture(&fixture, FW_FLOW_BASELINE_MIDDLE);
    child_state.fallback = 1u;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_OK);
    CHECK(sink_state.kinds[1] == FW_FLOW_FRAGMENT_PLACEHOLDER);
    CHECK(fabsf(sink_state.bounds[1].width - 24.0f) < 0.001f);
    CHECK(fabsf(sink_state.bounds[1].height - 16.0f) < 0.001f);
    return 1;
}

static int test_unsupported_slice(const fw_flow_layout_api_v1 *api,
    fw_plugin_handle handle) {
    flow_fixture fixture;
    fake_text_state text_state = {0};
    fake_child_state child_state = {0};
    fake_sink_state sink_state = {0};
    fw_flow_layout_result_v1 result = {0};
    make_fixture(&fixture, "demo", "p1", "hero", "p2", "Hello", "World");
    fixture.items[1].placement.mode = FW_FLOW_PLACE_FLOAT_START;
    CHECK(compose_fixture(api, handle, &fixture, &text_state, &child_state,
        &sink_state, &result) == FW_STATUS_UNSUPPORTED);
    CHECK(sink_state.begin_count == 0u);
    return 1;
}

int main(void) {
    const fw_plugin_api_v1 *plugin = facetwire_flow_layout_plugin_query(
        FW_ABI_VERSION_CURRENT);
    const fw_flow_layout_api_v1 *api;
    const void *interface_value = NULL;
    fw_host_api_v1 host;
    fw_plugin_handle handle = NULL;
    int passed = 1;
    CHECK(plugin != NULL);
    memset(&host, 0, sizeof(host));
    host.struct_size = sizeof(host);
    host.abi_version = FW_ABI_VERSION_CURRENT;
    CHECK(plugin->load(&host, &handle) == FW_STATUS_OK && handle != NULL);
    CHECK(plugin->query_interface(handle, view_of(FW_FLOW_LAYOUT_INTERFACE_ID),
        1u, &interface_value) == FW_STATUS_OK);
    api = (const fw_flow_layout_api_v1 *)interface_value;
    CHECK(api != NULL && api->interface_version == 1u);
    passed &= test_descriptor_and_validation(plugin, handle, api);
    passed &= test_continuous_block_order(api, handle);
    passed &= test_deterministic_key(api, handle);
    passed &= test_long_id_and_fallback(api, handle);
    passed &= test_failures_are_balanced(api, handle);
    passed &= test_virtual_pages_block_pagination(api, handle);
    passed &= test_paragraph_ranges_cross_pages(api, handle);
    passed &= test_virtual_page_budget_is_balanced(api, handle);
    passed &= test_virtual_page_retries_text_on_fresh_page(api, handle);
    passed &= test_virtual_page_rejects_zero_sized_object(api, handle);
    passed &= test_columns_block_progression(api, handle);
    passed &= test_paragraph_ranges_cross_columns_and_pages(api, handle);
    passed &= test_columns_page_budget_is_balanced(api, handle);
    passed &= test_invalid_column_geometry(api, handle);
    passed &= test_inline_object_continuous_order(api, handle);
    passed &= test_legacy_block_text_service_abi(api, handle);
    passed &= test_inline_baseline_modes(api, handle);
    passed &= test_inline_rtl_geometry(api, handle);
    passed &= test_inline_parts_cross_pages_and_columns(api, handle);
    passed &= test_inline_capability_and_part_validation(api, handle);
    passed &= test_inline_fallback_preserves_slot(api, handle);
    passed &= test_unsupported_slice(api, handle);
    plugin->unload(handle);
    if (!passed) return 1;
    {
        uint32_t iteration;
        for (iteration = 0u; iteration < 64u; ++iteration) {
            handle = NULL;
            CHECK(plugin->load(&host, &handle) == FW_STATUS_OK);
            CHECK(handle != NULL);
            plugin->unload(handle);
        }
    }
    puts("FacetWire Flow Layout contract tests passed.");
    return 0;
}
