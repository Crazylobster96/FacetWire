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

typedef struct fake_sink {
    uint32_t attempts;
    uint32_t commands;
    uint32_t depth;
    uint32_t fail_at;
    float last_fill_alpha;
} fake_sink;

typedef struct fake_text {
    uint32_t layouts;
    uint32_t releases;
} fake_text;

static fw_status fake_command(fake_sink *sink) {
    ++sink->attempts;
    if (sink->fail_at != 0u && sink->attempts == sink->fail_at) {
        return FW_STATUS_PLUGIN_ERROR;
    }
    ++sink->commands;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_save(void *user_data) {
    fake_sink *sink = (fake_sink *)user_data;
    const fw_status status = fake_command(sink);
    if (status == FW_STATUS_OK) {
        ++sink->depth;
    }
    return status;
}

static fw_status FW_CALL fake_restore(void *user_data) {
    fake_sink *sink = (fake_sink *)user_data;
    const fw_status status = fake_command(sink);
    if (status == FW_STATUS_OK && sink->depth > 0u) {
        --sink->depth;
    }
    return status;
}

static fw_status FW_CALL fake_clip(void *user_data, fw_rect_f32 rect) {
    (void)rect;
    return fake_command((fake_sink *)user_data);
}

static fw_status FW_CALL fake_fill(
    void *user_data,
    fw_rect_f32 rect,
    float radius,
    fw_color_rgba_f32 color) {
    fake_sink *sink = (fake_sink *)user_data;
    (void)rect;
    (void)radius;
    sink->last_fill_alpha = color.alpha;
    return fake_command(sink);
}

static fw_status FW_CALL fake_stroke(
    void *user_data,
    fw_rect_f32 rect,
    float radius,
    const fw_stroke_style_v1 *style) {
    (void)rect;
    (void)radius;
    (void)style;
    return fake_command((fake_sink *)user_data);
}

static fw_status FW_CALL fake_symbol(
    void *user_data,
    fw_string_view symbol_id,
    fw_rect_f32 rect,
    fw_color_rgba_f32 color) {
    (void)symbol_id;
    (void)rect;
    (void)color;
    return fake_command((fake_sink *)user_data);
}

static fw_status FW_CALL fake_draw_text(
    void *user_data,
    fw_text_layout_handle layout,
    fw_point_f32 origin,
    fw_color_rgba_f32 color) {
    (void)layout;
    (void)origin;
    (void)color;
    return fake_command((fake_sink *)user_data);
}

static fw_status FW_CALL fake_layout(
    void *user_data,
    const fw_text_layout_request_v1 *request,
    fw_text_layout_handle *out_layout,
    fw_text_layout_metrics_v1 *out_metrics) {
    fake_text *text = (fake_text *)user_data;
    if (request == NULL || out_layout == NULL || out_metrics == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    ++text->layouts;
    out_metrics->size.width = request->max_width;
    out_metrics->size.height = request->font_size * 1.2f;
    out_metrics->baseline = request->font_size;
    out_metrics->line_count = 1u;
    *out_layout = text;
    return FW_STATUS_OK;
}

static void FW_CALL fake_release(
    void *user_data,
    fw_text_layout_handle layout) {
    fake_text *text = (fake_text *)user_data;
    if (layout == text) {
        ++text->releases;
    }
}

static fw_placeholder_request_v1 default_request(void) {
    fw_placeholder_request_v1 request;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.request_id = 42u;
    request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:preview");
    request.content_kind = (fw_string_view)FW_STRING_VIEW_LITERAL("image");
    request.required_capability_id =
        (fw_string_view)FW_STRING_VIEW_LITERAL("facetwire.renderer.image");
    request.accessible_label =
        (fw_string_view)FW_STRING_VIEW_LITERAL("Generated preview");
    request.diagnostic_code =
        (fw_string_view)FW_STRING_VIEW_LITERAL("artifact.pending");
    request.reason = FW_PLACEHOLDER_REASON_RESOURCE_MISSING;
    request.mode = FW_PLACEHOLDER_MODE_STANDARD;
    request.permitted_actions = FW_PLACEHOLDER_ACTION_RETRY |
        FW_PLACEHOLDER_ACTION_SHOW_DETAILS;
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.max_width = 1000.0f;
    request.constraints.max_height = 1000.0f;
    request.constraints.em_size = 16.0f;
    request.constraints.line_height = 19.2f;
    request.style.struct_size = sizeof(request.style);
    request.style.background = (fw_color_rgba_f32){0.2f, 0.3f, 0.4f, 0.5f};
    request.style.border = (fw_color_rgba_f32){0.4f, 0.5f, 0.6f, 1.0f};
    request.style.icon = (fw_color_rgba_f32){0.7f, 0.7f, 0.8f, 1.0f};
    request.style.primary_text =
        (fw_color_rgba_f32){0.9f, 0.9f, 0.9f, 1.0f};
    request.style.action = (fw_color_rgba_f32){0.2f, 0.5f, 0.9f, 1.0f};
    request.style.opacity = 0.5f;
    request.style.border_width = 1.0f;
    request.style.corner_radius = 8.0f;
    request.style.content_padding = 8.0f;
    request.style.gap = 4.0f;
    request.style.icon_size = 32.0f;
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.fragment_count = 1u;
    request.presentation_revision = 7u;
    request.phase = FW_PLACEHOLDER_PHASE_RUNNING;
    request.progress.struct_size = sizeof(request.progress);
    request.progress.kind = FW_PLACEHOLDER_PROGRESS_FRACTION;
    request.progress.completed = 25u;
    request.progress.total = 100u;
    return request;
}

static fw_display_list_sink_v1 make_sink(fake_sink *state) {
    fw_display_list_sink_v1 sink;
    memset(&sink, 0, sizeof(sink));
    sink.struct_size = sizeof(sink);
    sink.user_data = state;
    sink.save = fake_save;
    sink.restore = fake_restore;
    sink.clip_rect = fake_clip;
    sink.fill_rounded_rect = fake_fill;
    sink.stroke_rounded_rect = fake_stroke;
    sink.draw_symbol = fake_symbol;
    sink.draw_text_layout = fake_draw_text;
    return sink;
}

static fw_text_service_v1 make_text(fake_text *state) {
    fw_text_service_v1 text;
    memset(&text, 0, sizeof(text));
    text.struct_size = sizeof(text);
    text.user_data = state;
    text.layout_utf8 = fake_layout;
    text.release_layout = fake_release;
    return text;
}

int main(void) {
    const fw_plugin_api_v1 *plugin_api;
    const fw_plugin_descriptor_v1 *descriptor;
    const fw_placeholder_renderer_api_v1 *renderer;
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle plugin = NULL;
    const void *interface_value = NULL;
    fw_placeholder_request_v1 request = default_request();
    fw_placeholder_validation_result_v1 validation = {0};
    fw_placeholder_measure_result_v1 measurement = {
        sizeof(fw_placeholder_measure_result_v1), {0.0f, 0.0f}, 0u, 0u, 0u};
    fw_placeholder_render_result_v1 render_result = {0};
    fw_placeholder_semantics_v1 semantics = {0};
    fw_placeholder_hit_test_request_v1 hit_request;
    fw_placeholder_hit_test_result_v1 hit_result = {0};
    fw_string_view schema = {NULL, 0u};
    fake_sink sink_state = {0};
    fake_text text_state = {0};
    fw_display_list_sink_v1 sink = make_sink(&sink_state);
    fw_text_service_v1 text = make_text(&text_state);
    fw_placeholder_services_v1 services;
    const fw_rect_f32 bounds = {0.0f, 0.0f, 320.0f, 180.0f};

    validation.struct_size = sizeof(validation);
    measurement.struct_size = sizeof(measurement);
    render_result.struct_size = sizeof(render_result);
    semantics.struct_size = sizeof(semantics);
    hit_result.struct_size = sizeof(hit_result);

    plugin_api = facetwire_placeholder_renderer_plugin_query(
        FW_ABI_VERSION_CURRENT);
    CHECK(plugin_api != NULL);
    CHECK(facetwire_placeholder_renderer_plugin_query(
        (fw_abi_version){2u, 0u}) == NULL);
    descriptor = plugin_api->get_descriptor();
    CHECK(descriptor != NULL);
    CHECK(descriptor->capability_count == 1u);
    CHECK(plugin_api->load(&host, &plugin) == FW_STATUS_OK);
    CHECK(plugin != NULL);
    CHECK(plugin_api->query_interface(
        plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(
            FW_PLACEHOLDER_RENDERER_INTERFACE_ID),
        1u,
        &interface_value) == FW_STATUS_OK);
    renderer = (const fw_placeholder_renderer_api_v1 *)interface_value;
    CHECK(renderer != NULL);
    CHECK(renderer->interface_version == 1u);

    CHECK(renderer->validate(plugin, &request, &validation) == FW_STATUS_OK);
    CHECK(validation.status == FW_STATUS_OK);
    CHECK(validation.normalization_flags == FW_PH_NORMALIZED_NONE);

    request.reason = 999u;
    request.mode = 999u;
    request.permitted_actions |= (1u << 31);
    request.phase = 999u;
    request.stale = 2u;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) == FW_STATUS_OK);
    CHECK((validation.normalization_flags & FW_PH_NORMALIZED_REASON) != 0u);
    CHECK((validation.normalization_flags & FW_PH_NORMALIZED_MODE) != 0u);
    CHECK((validation.normalization_flags & FW_PH_NORMALIZED_ACTIONS) != 0u);
    CHECK((validation.normalization_flags & FW_PH_NORMALIZED_AVAILABILITY) != 0u);
    request = default_request();
    request.progress.total = 0u;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);

    request = default_request();
    measurement.struct_size = sizeof(measurement);
    CHECK(renderer->measure(plugin, &request, &measurement) == FW_STATUS_OK);
    CHECK(measurement.source == FW_PH_MEASURE_KIND_FALLBACK);
    CHECK(fabsf(measurement.size.width - 256.0f) < 0.001f);
    CHECK(fabsf(measurement.size.height - 144.0f) < 0.001f);
    request.resolved_size.has_value = 1u;
    request.resolved_size.value = (fw_size_f32){123.0f, 45.0f};
    measurement.struct_size = sizeof(measurement);
    CHECK(renderer->measure(plugin, &request, &measurement) == FW_STATUS_OK);
    CHECK(measurement.source == FW_PH_MEASURE_RESOLVED);
    CHECK(measurement.size.width == 123.0f);
    CHECK(measurement.size.height == 45.0f);

    request = default_request();
    memset(&services, 0, sizeof(services));
    services.struct_size = sizeof(services);
    services.display_list = &sink;
    services.text = &text;
    services.locale = (fw_string_view)FW_STRING_VIEW_LITERAL("en-US");
    render_result.struct_size = sizeof(render_result);
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &render_result) == FW_STATUS_OK);
    CHECK(render_result.emitted_command_count == sink_state.commands);
    CHECK(render_result.visual_density == FW_PH_VISUAL_ACTIONS);
    CHECK(render_result.visible_actions == FW_PLACEHOLDER_ACTION_RETRY);
    CHECK(sink_state.depth == 0u);
    CHECK(fabsf(sink_state.last_fill_alpha - 0.5f) < 0.001f);
    CHECK(text_state.layouts == 1u);
    CHECK(text_state.releases == 1u);
    CHECK(render_result.cache_key_high != 0u);

    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(
        plugin, &request, bounds, &semantics) == FW_STATUS_OK);
    CHECK(semantics.role == FW_SEMANTICS_ROLE_IMAGE);
    CHECK(semantics.phase == FW_PLACEHOLDER_PHASE_RUNNING);
    CHECK(semantics.available_actions ==
        (FW_PLACEHOLDER_ACTION_RETRY |
         FW_PLACEHOLDER_ACTION_SHOW_DETAILS));

    memset(&hit_request, 0, sizeof(hit_request));
    hit_request.struct_size = sizeof(hit_request);
    hit_request.placeholder = request;
    hit_request.bounds = bounds;
    hit_request.point = (fw_point_f32){10.0f, 140.0f};
    hit_result.struct_size = sizeof(hit_result);
    CHECK(renderer->hit_test(plugin, &hit_request, &hit_result) == FW_STATUS_OK);
    CHECK(hit_result.hit == 1u);
    CHECK(hit_result.action == FW_PLACEHOLDER_ACTION_RETRY);

    request.mode = FW_PLACEHOLDER_MODE_HIDDEN;
    memset(&sink_state, 0, sizeof(sink_state));
    render_result.struct_size = sizeof(render_result);
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &render_result) == FW_STATUS_OK);
    CHECK(render_result.emitted_command_count == 0u);
    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(
        plugin, &request, bounds, &semantics) == FW_STATUS_OK);
    CHECK(semantics.hidden_visually == 1u);

    request = default_request();
    memset(&sink_state, 0, sizeof(sink_state));
    sink_state.fail_at = 3u;
    render_result.struct_size = sizeof(render_result);
    CHECK(renderer->render(
        plugin, &request, bounds, &services, &render_result) ==
        FW_STATUS_SINK_REJECTED);
    CHECK(sink_state.depth == 0u);
    CHECK(render_result.emitted_command_count == 0u);
    CHECK(render_result.visual_density == FW_PH_VISUAL_NONE);
    CHECK(render_result.visible_actions == FW_PLACEHOLDER_ACTION_NONE);
    CHECK(render_result.cache_key_high == 0u);
    CHECK(render_result.cache_key_low == 0u);

    CHECK(renderer->get_parameter_schema(plugin, &schema) == FW_STATUS_OK);
    CHECK(schema.data != NULL && schema.length > 0u);
    CHECK(memchr(schema.data, '{', schema.length) != NULL);

    plugin_api->unload(plugin);
    puts("FacetWire placeholder renderer tests passed.");
    return 0;
}
