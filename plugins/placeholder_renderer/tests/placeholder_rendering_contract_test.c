/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/placeholder_renderer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                    \
    do {                                                                     \
        if (!(expression)) {                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                \
                __FILE__, __LINE__, #expression);                            \
            return 1;                                                        \
        }                                                                    \
    } while (0)

typedef struct recording_sink {
    uint32_t commands;
    uint32_t depth;
    uint32_t fill_count;
    float first_fill_alpha;
} recording_sink;

static fw_status record_command(recording_sink *sink) {
    ++sink->commands;
    return FW_STATUS_OK;
}

static fw_status FW_CALL record_save(void *user_data) {
    recording_sink *sink = (recording_sink *)user_data;
    ++sink->depth;
    return record_command(sink);
}

static fw_status FW_CALL record_restore(void *user_data) {
    recording_sink *sink = (recording_sink *)user_data;
    if (sink->depth > 0u) {
        --sink->depth;
    }
    return record_command(sink);
}

static fw_status FW_CALL record_clip(void *user_data, fw_rect_f32 rect) {
    (void)rect;
    return record_command((recording_sink *)user_data);
}

static fw_status FW_CALL record_fill(
    void *user_data,
    fw_rect_f32 rect,
    float radius,
    fw_color_rgba_f32 color) {
    recording_sink *sink = (recording_sink *)user_data;
    (void)rect;
    (void)radius;
    if (sink->fill_count == 0u) {
        sink->first_fill_alpha = color.alpha;
    }
    ++sink->fill_count;
    return record_command(sink);
}

static fw_status FW_CALL record_stroke(
    void *user_data,
    fw_rect_f32 rect,
    float radius,
    const fw_stroke_style_v1 *style) {
    (void)rect;
    (void)radius;
    (void)style;
    return record_command((recording_sink *)user_data);
}

static fw_status FW_CALL record_symbol(
    void *user_data,
    fw_string_view symbol_id,
    fw_rect_f32 rect,
    fw_color_rgba_f32 color) {
    (void)symbol_id;
    (void)rect;
    (void)color;
    return record_command((recording_sink *)user_data);
}

static fw_status FW_CALL record_text(
    void *user_data,
    fw_text_layout_handle layout,
    fw_point_f32 origin,
    fw_color_rgba_f32 color) {
    (void)layout;
    (void)origin;
    (void)color;
    return record_command((recording_sink *)user_data);
}

static fw_placeholder_request_v1 make_request(void) {
    fw_placeholder_request_v1 request;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.request_id = 1u;
    request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:test");
    request.content_kind = (fw_string_view)FW_STRING_VIEW_LITERAL("image");
    request.reason = FW_PLACEHOLDER_REASON_LOADING;
    request.mode = FW_PLACEHOLDER_MODE_MINIMAL;
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.max_width = 1000.0f;
    request.constraints.max_height = 1000.0f;
    request.constraints.em_size = 16.0f;
    request.constraints.line_height = 19.2f;
    request.style.struct_size = sizeof(request.style);
    request.style.opacity = 1.0f;
    request.style.corner_radius = 8.0f;
    request.style.content_padding = 8.0f;
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.fragment_count = 1u;
    request.presentation_revision = 10u;
    request.phase = FW_PLACEHOLDER_PHASE_RUNNING;
    request.progress.struct_size = sizeof(request.progress);
    request.progress.kind = FW_PLACEHOLDER_PROGRESS_INDETERMINATE;
    return request;
}

static fw_display_list_sink_v1 make_sink(recording_sink *state) {
    fw_display_list_sink_v1 sink;
    memset(&sink, 0, sizeof(sink));
    sink.struct_size = sizeof(sink);
    sink.user_data = state;
    sink.save = record_save;
    sink.restore = record_restore;
    sink.clip_rect = record_clip;
    sink.fill_rounded_rect = record_fill;
    sink.stroke_rounded_rect = record_stroke;
    sink.draw_symbol = record_symbol;
    sink.draw_text_layout = record_text;
    return sink;
}

int main(void) {
    const fw_plugin_api_v1 *plugin_api =
        facetwire_placeholder_renderer_plugin_query(FW_ABI_VERSION_CURRENT);
    const fw_placeholder_renderer_api_v1 *renderer;
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle plugin = NULL;
    const void *interface_value = NULL;
    fw_placeholder_request_v1 request = make_request();
    recording_sink sink_state = {0};
    fw_display_list_sink_v1 sink = make_sink(&sink_state);
    fw_placeholder_services_v1 services;
    fw_placeholder_render_result_v1 first = {0};
    fw_placeholder_render_result_v1 second = {0};
    const fw_rect_f32 bounds = {0.0f, 0.0f, 320.0f, 180.0f};

    first.struct_size = sizeof(first);
    second.struct_size = sizeof(second);

    CHECK(plugin_api != NULL);
    CHECK(plugin_api->load(&host, &plugin) == FW_STATUS_OK);
    CHECK(plugin_api->query_interface(
        plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(
            FW_PLACEHOLDER_RENDERER_INTERFACE_ID),
        1u,
        &interface_value) == FW_STATUS_OK);
    renderer = (const fw_placeholder_renderer_api_v1 *)interface_value;

    memset(&services, 0, sizeof(services));
    services.struct_size = sizeof(services);
    services.display_list = &sink;

    /* A zero-alpha background is not replaced with an opaque fallback. */
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &first) == FW_STATUS_OK);
    CHECK(sink_state.fill_count == 0u);
    CHECK(sink_state.depth == 0u);

    /* User-facing opacity is multiplied into color alpha exactly once. */
    memset(&sink_state, 0, sizeof(sink_state));
    request.style.background.alpha = 0.8f;
    request.style.opacity = 0.25f;
    first.struct_size = sizeof(first);
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &first) == FW_STATUS_OK);
    CHECK(sink_state.fill_count == 1u);
    CHECK(fabsf(sink_state.first_fill_alpha - 0.2f) < 0.0001f);

    /* Session revision changes cache identity, never geometry or command shape. */
    memset(&sink_state, 0, sizeof(sink_state));
    request.presentation_revision = 11u;
    second.struct_size = sizeof(second);
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &second) == FW_STATUS_OK);
    CHECK(second.emitted_command_count == first.emitted_command_count);
    CHECK(second.visual_density == first.visual_density);
    CHECK(second.cache_key_high != first.cache_key_high);
    CHECK(second.cache_key_low != first.cache_key_low);

    /* Every rendering input that changes output participates in cache identity. */
    request.style.opacity = 0.5f;
    memset(&sink_state, 0, sizeof(sink_state));
    first.struct_size = sizeof(first);
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &first) == FW_STATUS_OK);
    CHECK(first.cache_key_high != second.cache_key_high);
    CHECK(first.cache_key_low != second.cache_key_low);

    /* A transparent action cannot remain visible or produce a hit region. */
    {
        fw_placeholder_hit_test_request_v1 hit_request;
        fw_placeholder_hit_test_result_v1 hit_result = {0};
        request.reason = FW_PLACEHOLDER_REASON_RESOURCE_MISSING;
        request.mode = FW_PLACEHOLDER_MODE_STANDARD;
        request.permitted_actions = FW_PLACEHOLDER_ACTION_RETRY;
        request.style.action.alpha = 0.0f;
        memset(&sink_state, 0, sizeof(sink_state));
        first.struct_size = sizeof(first);
        CHECK(renderer->render(
            plugin, &request, bounds, &services, &first) == FW_STATUS_OK);
        CHECK(first.visual_density == FW_PH_VISUAL_ACTIONS);
        CHECK(first.visible_actions == FW_PLACEHOLDER_ACTION_NONE);

        memset(&hit_request, 0, sizeof(hit_request));
        hit_request.struct_size = sizeof(hit_request);
        hit_request.placeholder = request;
        hit_request.bounds = bounds;
        hit_request.point = (fw_point_f32){10.0f, 140.0f};
        hit_result.struct_size = sizeof(hit_result);
        CHECK(renderer->hit_test(
            plugin, &hit_request, &hit_result) == FW_STATUS_OK);
        CHECK(hit_result.hit == 0u);
    }

    plugin_api->unload(plugin);
    puts("FacetWire placeholder rendering contract tests passed.");
    return 0;
}
