/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_ui_spike.h"

#include <facetwire/child_measure_service.h>
#include <facetwire/flow_layout.h>
#include <facetwire/text_fragment_service.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FWDL_HEADER_SIZE 12u
#define FWDL_COMMAND_SIZE 40u
#define FWDL_COMMAND_COUNT 3u
#define FWUI_FLOW_MAX_FRAGMENTS 16u
#define FWUI_FLOW_JSON_CAPACITY 8192u

struct fwui_context {
    uint32_t abi_version;
    const fw_plugin_api_v1 *flow_plugin;
    fw_plugin_handle flow_handle;
    const fw_flow_layout_api_v1 *flow;
};

typedef struct fwui_flow_case_state {
    uint32_t demo_case;
} fwui_flow_case_state;

typedef struct fwui_flow_fragment_record {
    fw_flow_fragment_kind kind;
    char source_item_id[96];
    char content_kind[48];
    fw_rect_f32 bounds;
    size_t text_start;
    size_t text_end;
} fwui_flow_fragment_record;

typedef struct fwui_flow_sink_state {
    uint32_t begin_count;
    uint32_t end_count;
    uint32_t fragment_count;
    fw_flow_page_v1 page;
    fwui_flow_fragment_record fragments[FWUI_FLOW_MAX_FRAGMENTS];
} fwui_flow_sink_state;

static int buffer_available(const fwui_buffer *buffer) {
    return buffer != NULL && buffer->data == NULL && buffer->length == 0u;
}

static fw_string_view string_view(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static void write_u16_le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)((value >> 8u) & 0xffu);
}

static void write_u32_le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)((value >> 8u) & 0xffu);
    dst[2] = (uint8_t)((value >> 16u) & 0xffu);
    dst[3] = (uint8_t)((value >> 24u) & 0xffu);
}

static void write_f32_le(uint8_t *dst, float value) {
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(dst, bits);
}

static void write_command(
    uint8_t *dst,
    uint8_t opcode,
    float x,
    float y,
    float width,
    float height,
    float radius,
    float red,
    float green,
    float blue,
    float alpha) {
    const float values[9] = {
        x, y, width, height, radius, red, green, blue, alpha
    };
    size_t index = 0u;
    dst[0] = opcode;
    dst[1] = 0u;
    dst[2] = 0u;
    dst[3] = 0u;
    for (index = 0u; index < 9u; ++index) {
        write_f32_le(dst + 4u + (index * 4u), values[index]);
    }
}

static fwui_status copy_bytes(
    const void *data,
    size_t length,
    fwui_buffer *out_buffer) {
    uint8_t *copy = NULL;
    if ((data == NULL && length != 0u) || !buffer_available(out_buffer)) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }
    copy = (uint8_t *)calloc(length + 1u, 1u);
    if (copy == NULL && length != 0u) {
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    if (length != 0u) memcpy(copy, data, length);
    out_buffer->data = copy;
    out_buffer->length = (uint64_t)length;
    return FWUI_STATUS_OK;
}

static fwui_status copy_text(const char *text, fwui_buffer *out_buffer) {
    if (text == NULL) return FWUI_STATUS_INVALID_ARGUMENT;
    return copy_bytes(text, strlen(text), out_buffer);
}

static fw_status FW_CALL flow_measure_text(
    void *user_data,
    const fw_text_fragment_request_v1 *request,
    fw_text_fragment_metrics_v1 *out_metrics) {
    size_t total = 0u;
    size_t index;
    (void)user_data;
    if (request == NULL || out_metrics == NULL ||
        out_metrics->struct_size < sizeof(*out_metrics)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < request->segment_count; ++index) {
        if (request->segments[index].kind == FW_FLOW_SEGMENT_TEXT)
            total += request->segments[index].text.length;
    }
    if (request->start_utf8_byte > total || request->region.height < 56.0f)
        return FW_STATUS_RESOURCE_LIMIT;
    out_metrics->end_utf8_byte = total;
    out_metrics->used_bounds.x = request->region.x;
    out_metrics->used_bounds.y = request->region.y;
    out_metrics->used_bounds.width = request->region.width;
    out_metrics->used_bounds.height = 56.0f;
    out_metrics->line_count = 2u;
    out_metrics->reached_end = 1u;
    out_metrics->fingerprint_high = UINT64_C(0x4657554954455854);
    out_metrics->fingerprint_low = (uint64_t)total;
    return FW_STATUS_OK;
}

static fw_status FW_CALL flow_draw_text(
    void *user_data,
    const fw_text_fragment_request_v1 *request,
    const fw_text_fragment_metrics_v1 *expected,
    const fw_display_list_sink_v1 *display_list) {
    (void)user_data;
    (void)request;
    (void)expected;
    (void)display_list;
    return FW_STATUS_UNSUPPORTED;
}

static fw_status FW_CALL flow_measure_child(
    void *user_data,
    const fw_child_measure_request_v1 *request,
    fw_child_measure_result_v1 *out_result) {
    const fwui_flow_case_state *state =
        (const fwui_flow_case_state *)user_data;
    (void)request;
    if (state == NULL || out_result == NULL ||
        out_result->struct_size < sizeof(*out_result)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    out_result->intrinsic_size.width = 240.0f - (20.0f * state->demo_case);
    out_result->intrinsic_size.height = 150.0f - (15.0f * state->demo_case);
    out_result->fallback_size.width = 180.0f;
    out_result->fallback_size.height = 112.0f;
    out_result->aspect_ratio.has_value = 1u;
    out_result->aspect_ratio.value = state->demo_case == 2u ?
        (180.0f / 112.0f) :
        (out_result->intrinsic_size.width / out_result->intrinsic_size.height);
    out_result->has_intrinsic_size = state->demo_case == 2u ? 0u : 1u;
    out_result->used_fallback = state->demo_case == 2u ? 1u : 0u;
    out_result->fingerprint_high = UINT64_C(0x465755494348494c);
    out_result->fingerprint_low = (uint64_t)state->demo_case;
    return FW_STATUS_OK;
}

static fw_status FW_CALL flow_begin_page(
    void *user_data,
    const fw_flow_page_v1 *page) {
    fwui_flow_sink_state *state = (fwui_flow_sink_state *)user_data;
    if (state == NULL || page == NULL || state->begin_count != 0u)
        return FW_STATUS_SINK_REJECTED;
    state->page = *page;
    state->begin_count = 1u;
    return FW_STATUS_OK;
}

static void copy_view(char *target, size_t capacity, fw_string_view source) {
    size_t length = source.length;
    if (capacity == 0u) return;
    if (length >= capacity) length = capacity - 1u;
    if (length != 0u) memcpy(target, source.data, length);
    target[length] = '\0';
}

static fw_status FW_CALL flow_emit_fragment(
    void *user_data,
    const fw_flow_fragment_v1 *fragment) {
    fwui_flow_sink_state *state = (fwui_flow_sink_state *)user_data;
    fwui_flow_fragment_record *record;
    if (state == NULL || fragment == NULL ||
        state->fragment_count >= FWUI_FLOW_MAX_FRAGMENTS) {
        return FW_STATUS_CAPACITY_EXCEEDED;
    }
    record = &state->fragments[state->fragment_count++];
    record->kind = fragment->kind;
    copy_view(record->source_item_id, sizeof(record->source_item_id),
        fragment->source_item_id);
    copy_view(record->content_kind, sizeof(record->content_kind),
        fragment->content_kind);
    record->bounds = fragment->bounds;
    record->text_start = fragment->text_start_utf8_byte;
    record->text_end = fragment->text_end_utf8_byte;
    return FW_STATUS_OK;
}

static fw_status FW_CALL flow_end_page(void *user_data, uint32_t page_index) {
    fwui_flow_sink_state *state = (fwui_flow_sink_state *)user_data;
    if (state == NULL || page_index != 0u || state->begin_count != 1u ||
        state->end_count != 0u) return FW_STATUS_SINK_REJECTED;
    state->end_count = 1u;
    return FW_STATUS_OK;
}

static int append_json(
    char *buffer,
    size_t capacity,
    size_t *offset,
    const char *format,
    ...) {
    int written;
    va_list arguments;
    if (*offset >= capacity) return 0;
    va_start(arguments, format);
    written = vsnprintf(buffer + *offset, capacity - *offset, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *offset) return 0;
    *offset += (size_t)written;
    return 1;
}

static const char *fragment_kind_name(fw_flow_fragment_kind kind) {
    if (kind == FW_FLOW_FRAGMENT_TEXT) return "text";
    if (kind == FW_FLOW_FRAGMENT_OBJECT) return "object";
    return "placeholder";
}

static fwui_status serialize_flow_report(
    uint32_t demo_case,
    fw_status compose_status,
    const fw_flow_layout_result_v1 *result,
    const fwui_flow_sink_state *sink,
    fwui_buffer *out_buffer) {
    char json[FWUI_FLOW_JSON_CAPACITY];
    size_t offset = 0u;
    uint32_t index;
    if (!append_json(json, sizeof(json), &offset,
        "{\"pluginId\":\"org.facetwire.reference.flow-layout\","
        "\"capability\":\"facetwire.layout.flow\","
        "\"interfaceVersion\":1,\"demoCase\":%u,\"composeStatus\":%u,"
        "\"complete\":%s,\"pageCount\":%u,\"fragmentCount\":%u,"
        "\"textFragmentCount\":%u,\"objectFragmentCount\":%u,"
        "\"continuousExtent\":{\"width\":%.3f,\"height\":%.3f},"
        "\"planKey\":\"%016llx%016llx\",\"pagesBalanced\":%s,"
        "\"supportedSlice\":\"continuous+block\",\"nativeRuntime\":true,\"fragments\":[",
        demo_case, (unsigned int)compose_status,
        result->complete != 0u ? "true" : "false", result->page_count,
        result->fragment_count, result->text_fragment_count,
        result->object_fragment_count, result->continuous_extent.width,
        result->continuous_extent.height,
        (unsigned long long)result->plan_key_high,
        (unsigned long long)result->plan_key_low,
        sink->begin_count == sink->end_count ? "true" : "false")) {
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    for (index = 0u; index < sink->fragment_count; ++index) {
        const fwui_flow_fragment_record *fragment = &sink->fragments[index];
        if (!append_json(json, sizeof(json), &offset,
            "%s{\"kind\":\"%s\",\"sourceItemId\":\"%s\","
            "\"contentKind\":\"%s\",\"bounds\":{\"x\":%.3f,"
            "\"y\":%.3f,\"width\":%.3f,\"height\":%.3f},"
            "\"textStart\":%llu,\"textEnd\":%llu}",
            index == 0u ? "" : ",", fragment_kind_name(fragment->kind),
            fragment->source_item_id, fragment->content_kind,
            fragment->bounds.x, fragment->bounds.y, fragment->bounds.width,
            fragment->bounds.height,
            (unsigned long long)fragment->text_start,
            (unsigned long long)fragment->text_end)) {
            return FWUI_STATUS_OUT_OF_MEMORY;
        }
    }
    if (!append_json(json, sizeof(json), &offset, "]}"))
        return FWUI_STATUS_OUT_OF_MEMORY;
    return copy_bytes(json, offset, out_buffer);
}

fwui_status fwui_context_create(fwui_context **out_context) {
    fwui_context *context;
    fw_host_api_v1 host;
    const void *interface_value = NULL;
    fw_status status;
    if (out_context == NULL) return FWUI_STATUS_INVALID_ARGUMENT;
    *out_context = NULL;
    context = (fwui_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FWUI_STATUS_OUT_OF_MEMORY;
    context->abi_version = 1u;
    context->flow_plugin = facetwire_flow_layout_plugin_query(
        FW_ABI_VERSION_CURRENT);
    if (context->flow_plugin == NULL) {
        free(context);
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    memset(&host, 0, sizeof(host));
    host.struct_size = sizeof(host);
    host.abi_version = FW_ABI_VERSION_CURRENT;
    status = context->flow_plugin->load(&host, &context->flow_handle);
    if (status == FW_STATUS_OK) {
        status = context->flow_plugin->query_interface(context->flow_handle,
            string_view(FW_FLOW_LAYOUT_INTERFACE_ID),
            FW_FLOW_LAYOUT_INTERFACE_VERSION, &interface_value);
    }
    if (status != FW_STATUS_OK || interface_value == NULL) {
        if (context->flow_handle != NULL)
            context->flow_plugin->unload(context->flow_handle);
        free(context);
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    context->flow = (const fw_flow_layout_api_v1 *)interface_value;
    *out_context = context;
    return FWUI_STATUS_OK;
}

void fwui_context_destroy(fwui_context *context) {
    if (context != NULL) {
        if (context->flow_plugin != NULL && context->flow_handle != NULL)
            context->flow_plugin->unload(context->flow_handle);
        context->flow_handle = NULL;
        context->flow = NULL;
        free(context);
    }
}

fwui_status fwui_runtime_snapshot(
    fwui_context *context,
    fwui_buffer *out_utf8_json) {
    static const char snapshot[] =
        "{\"abiVersion\":1,\"state\":\"ready\",\"plugins\":["
        "\"placeholder\",\"flow-layout\"],"
        "\"flowCapability\":\"facetwire.layout.flow\","
        "\"flowInterfaceVersion\":1,"
        "\"flowSupportedSlice\":\"continuous+block\"}";
    if (context == NULL || context->flow == NULL ||
        !buffer_available(out_utf8_json) || context->abi_version != 1u) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }
    return copy_text(snapshot, out_utf8_json);
}

fwui_status fwui_render_placeholder(
    fwui_context *context,
    float width,
    float height,
    float opacity,
    fwui_buffer *out_display_list,
    fwui_buffer *out_semantics_utf8_json) {
    static const char semantics[] =
        "{\"revision\":1,\"nodes\":[{\"id\":1,\"role\":\"image\","
        "\"label\":\"Unsupported FacetWire zone placeholder\"}]}";
    const uint64_t byte_length = FWDL_HEADER_SIZE +
        (FWDL_COMMAND_SIZE * FWDL_COMMAND_COUNT);
    uint8_t *bytes;
    fwui_status status;
    float inset;
    float radius;
    float minimum_dimension;
    if (context == NULL || !buffer_available(out_display_list) ||
        !buffer_available(out_semantics_utf8_json) ||
        out_display_list == out_semantics_utf8_json ||
        context->abi_version != 1u || !isfinite(width) || !isfinite(height) ||
        !isfinite(opacity) || width <= 0.0f || height <= 0.0f ||
        opacity < 0.0f || opacity > 1.0f) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }
    minimum_dimension = width < height ? width : height;
    inset = minimum_dimension * 0.25f;
    radius = minimum_dimension * 0.20f;
    if (inset > 12.0f) inset = 12.0f;
    if (radius > 12.0f) radius = 12.0f;
    bytes = (uint8_t *)calloc((size_t)byte_length, 1u);
    if (bytes == NULL) return FWUI_STATUS_OUT_OF_MEMORY;
    memcpy(bytes, "FWDL", 4u);
    write_u16_le(bytes + 4u, 1u);
    write_u16_le(bytes + 6u, FWDL_HEADER_SIZE);
    write_u32_le(bytes + 8u, FWDL_COMMAND_COUNT);
    write_command(bytes + 12u, 1u, 0.0f, 0.0f, width, height, 0.0f,
        0.07f, 0.09f, 0.13f, opacity);
    write_command(bytes + 52u, 2u, inset, inset, width - (2.0f * inset),
        height - (2.0f * inset), radius, 0.25f, 0.52f, 0.96f,
        opacity * 0.30f);
    write_command(bytes + 92u, 3u, inset, inset, width - (2.0f * inset),
        height - (2.0f * inset), radius, 0.56f, 0.72f, 1.0f, opacity);
    out_display_list->data = bytes;
    out_display_list->length = byte_length;
    status = copy_text(semantics, out_semantics_utf8_json);
    if (status != FWUI_STATUS_OK) {
        fwui_buffer_release(out_display_list);
        return status;
    }
    return FWUI_STATUS_OK;
}

fwui_status fwui_compose_flow_demo(
    fwui_context *context,
    float width,
    float height,
    uint32_t demo_case,
    fwui_buffer *out_layout_plan_utf8_json) {
    static const char *const paragraph_ids[3] = {
        "paragraph.intro.level-1",
        "paragraph.intro.level-2",
        "paragraph.intro.level-3"
    };
    static const char *const object_ids[3] = {
        "image.hero.level-1", "image.hero.level-2", "object.missing.level-3"
    };
    static const char *const closing_ids[3] = {
        "paragraph.closing.level-1",
        "paragraph.closing.level-2",
        "paragraph.closing.level-3"
    };
    static const char *const intro_text[3] = {
        "Level 1 root Flow is composed by the native plugin.",
        "Level 2 nested Flow preserves order and collapsed margins.",
        "Level 3 fallback Flow preserves bounds for an unknown object."
    };
    static const char *const closing_text[3] = {
        "The image occupies flow space; this paragraph follows it.",
        "Native Layout Plan remains deterministic on every host.",
        "Fallback does not collapse the following paragraph."
    };
    fwui_flow_case_state case_state;
    fw_flow_segment_v1 intro_segment;
    fw_flow_segment_v1 closing_segment;
    fw_flow_item_v1 items[3];
    fw_flow_layout_request_v1 request;
    fw_text_fragment_service_v1 text_service;
    fw_child_measure_service_v1 child_service;
    fw_flow_layout_services_v1 services;
    fwui_flow_sink_state sink_state;
    fw_flow_plan_sink_v1 sink;
    fw_flow_layout_result_v1 result;
    fw_status status;
    uint32_t content_case;
    uint32_t index;
    if (context == NULL || context->flow == NULL ||
        !buffer_available(out_layout_plan_utf8_json) ||
        !isfinite(width) || !isfinite(height) || width < 240.0f ||
        height < 320.0f || demo_case > 3u) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }
    content_case = demo_case == 3u ? 0u : demo_case;
    case_state.demo_case = content_case;
    memset(&intro_segment, 0, sizeof(intro_segment));
    intro_segment.struct_size = sizeof(intro_segment);
    intro_segment.kind = FW_FLOW_SEGMENT_TEXT;
    intro_segment.text = string_view(intro_text[content_case]);
    memset(&closing_segment, 0, sizeof(closing_segment));
    closing_segment.struct_size = sizeof(closing_segment);
    closing_segment.kind = FW_FLOW_SEGMENT_TEXT;
    closing_segment.text = string_view(closing_text[content_case]);
    memset(items, 0, sizeof(items));
    for (index = 0u; index < 3u; ++index) {
        items[index].struct_size = sizeof(items[index]);
        items[index].placement.struct_size = sizeof(items[index].placement);
        items[index].placement.mode = FW_FLOW_PLACE_BLOCK;
        items[index].placement.max_width = width;
        items[index].placement.max_height = height;
        items[index].placement.allow_scale_down = 1u;
        items[index].break_policy.struct_size = sizeof(items[index].break_policy);
        items[index].break_policy.orphans = 2u;
        items[index].break_policy.widows = 2u;
    }
    items[0].id = string_view(paragraph_ids[content_case]);
    items[0].kind = FW_FLOW_ITEM_PARAGRAPH;
    items[0].segments = &intro_segment;
    items[0].segment_count = 1u;
    items[0].text_style.struct_size = sizeof(items[0].text_style);
    items[0].text_style.font_size = 20.0f;
    items[0].text_style.font_weight = 600u;
    items[0].text_style.line_height_multiplier = 1.35f;
    items[0].text_style.color.alpha = 1.0f;
    items[0].direction = FW_TEXT_DIRECTION_LTR;
    items[0].placement.margins.top = 8.0f;
    items[0].placement.margins.bottom = 8.0f;
    items[1].id = string_view(object_ids[content_case]);
    items[1].kind = FW_FLOW_ITEM_OBJECT;
    items[1].content_id = string_view(content_case == 2u ?
        "resource:missing" : "resource:hero");
    items[1].content_kind = string_view(content_case == 2u ?
        "unknown" : "image");
    items[1].placement.margins.top = 16.0f;
    items[1].placement.margins.bottom = 12.0f;
    items[2].id = string_view(closing_ids[content_case]);
    items[2].kind = FW_FLOW_ITEM_PARAGRAPH;
    items[2].segments = &closing_segment;
    items[2].segment_count = 1u;
    items[2].text_style = items[0].text_style;
    items[2].text_style.font_size = 16.0f;
    items[2].text_style.font_weight = 400u;
    items[2].direction = FW_TEXT_DIRECTION_LTR;
    items[2].placement.margins.top = 10.0f;
    items[2].placement.margins.bottom = 6.0f;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.request_id = UINT64_C(1000) + demo_case;
    request.flow_id = string_view(content_case == 0u ? "flow.level-1" :
        (content_case == 1u ? "flow.level-2" : "flow.level-3"));
    request.items = items;
    request.item_count = 3u;
    request.page_template.struct_size = sizeof(request.page_template);
    request.page_template.mode = demo_case == 3u ?
        FW_FLOW_VIRTUAL_PAGES : FW_FLOW_CONTINUOUS;
    request.page_template.page_size.width = width;
    request.page_template.page_size.height = height;
    request.page_template.margins.left = 24.0f;
    request.page_template.margins.top = 24.0f;
    request.page_template.margins.right = 24.0f;
    request.page_template.margins.bottom = 24.0f;
    request.page_template.column_count = 1u;
    request.page_template.minimum_text_width = 120.0f;
    request.budget.struct_size = sizeof(request.budget);
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.document_revision = 1u;
    request.layout_revision = 1u + demo_case;
    request.profile_key = string_view("playground-flow-demo");
    memset(&text_service, 0, sizeof(text_service));
    text_service.struct_size = sizeof(text_service);
    text_service.user_data = &case_state;
    text_service.measure_next = flow_measure_text;
    text_service.draw_exact = flow_draw_text;
    memset(&child_service, 0, sizeof(child_service));
    child_service.struct_size = sizeof(child_service);
    child_service.user_data = &case_state;
    child_service.measure_child = flow_measure_child;
    memset(&services, 0, sizeof(services));
    services.struct_size = sizeof(services);
    services.text = &text_service;
    services.children = &child_service;
    memset(&sink_state, 0, sizeof(sink_state));
    memset(&sink, 0, sizeof(sink));
    sink.struct_size = sizeof(sink);
    sink.user_data = &sink_state;
    sink.begin_page = flow_begin_page;
    sink.emit_fragment = flow_emit_fragment;
    sink.end_page = flow_end_page;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    status = context->flow->compose(context->flow_handle, &request, &services,
        &sink, &result);
    return serialize_flow_report(demo_case, status, &result, &sink_state,
        out_layout_plan_utf8_json);
}

void fwui_buffer_release(fwui_buffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->length = 0u;
    }
}