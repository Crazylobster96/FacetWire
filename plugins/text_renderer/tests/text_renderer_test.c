/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/text_renderer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #value); \
    return 1; } } while (0)

typedef struct fake_state {
    uint32_t layouts;
    uint32_t releases;
    uint32_t saves;
    uint32_t restores;
    uint32_t clips;
    uint32_t fills;
    uint32_t draws;
    float last_alpha;
    fw_text_layout_request_v2 last_request;
} fake_state;

static fw_status FW_CALL fake_layout(void *user_data,
    const fw_text_layout_request_v2 *request,
    fw_text_layout_handle *out_layout,
    fw_text_layout_metrics_v2 *out_metrics) {
    fake_state *state = (fake_state *)user_data;
    state->last_request = *request;
    ++state->layouts;
    *out_layout = malloc(1u);
    if (*out_layout == NULL) return FW_STATUS_OUT_OF_MEMORY;
    out_metrics->size.width = request->max_width < 180.0f ?
        request->max_width : 180.0f;
    out_metrics->size.height = 72.0f;
    out_metrics->ink_bounds = (fw_rect_f32){0, 0,
        out_metrics->size.width, out_metrics->size.height};
    out_metrics->first_baseline = 18.0f;
    out_metrics->last_baseline = 62.0f;
    out_metrics->line_count = 3u;
    out_metrics->resolved_font_key =
        (fw_string_view)FW_STRING_VIEW_LITERAL("fake-font-v1");
    return FW_STATUS_OK;
}

static void FW_CALL fake_release(void *user_data, fw_text_layout_handle layout) {
    fake_state *state = (fake_state *)user_data;
    ++state->releases;
    free(layout);
}

static fw_status FW_CALL fake_save(void *u) {
    ++((fake_state *)u)->saves; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_restore(void *u) {
    ++((fake_state *)u)->restores; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_clip(void *u, fw_rect_f32 rect) {
    (void)rect; ++((fake_state *)u)->clips; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_fill(void *u, fw_rect_f32 rect, float radius,
    fw_color_rgba_f32 color) {
    fake_state *state = (fake_state *)u;
    (void)rect; (void)radius; ++state->fills; state->last_alpha = color.alpha;
    return FW_STATUS_OK;
}
static fw_status FW_CALL fake_stroke(void *u, fw_rect_f32 rect, float radius,
    const fw_stroke_style_v1 *style) {
    (void)u; (void)rect; (void)radius; (void)style; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_symbol(void *u, fw_string_view id,
    fw_rect_f32 rect, fw_color_rgba_f32 color) {
    (void)u; (void)id; (void)rect; (void)color; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_draw(void *u, fw_text_layout_handle layout,
    fw_point_f32 origin, fw_color_rgba_f32 color) {
    fake_state *state = (fake_state *)u;
    (void)layout; (void)origin; ++state->draws; state->last_alpha = color.alpha;
    return FW_STATUS_OK;
}

static fw_text_renderer_request_v1 make_request(void) {
    fw_text_renderer_request_v1 request;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:text");
    request.text = (fw_string_view)FW_STRING_VIEW_LITERAL("第一行\r\nSecond line");
    request.language = (fw_string_view)FW_STRING_VIEW_LITERAL("zh-CN");
    request.direction = FW_TEXT_DIRECTION_AUTO;
    request.selectable = 1u;
    request.opacity = 0.5f;
    request.style.struct_size = sizeof(request.style);
    request.style.font_size = 20.0f;
    request.style.font_weight = 400u;
    request.style.font_style = FW_TEXT_FONT_NORMAL;
    request.style.line_height_multiplier = 1.2f;
    request.style.color = (fw_color_rgba_f32){0.1f, 0.2f, 0.3f, 0.8f};
    request.style.background_color =
        (fw_color_rgba_f32){1.0f, 1.0f, 1.0f, 0.4f};
    request.style.has_background_color = 1u;
    request.layout.struct_size = sizeof(request.layout);
    request.layout.horizontal_align = FW_TEXT_ALIGN_START;
    request.layout.vertical_align = FW_TEXT_ALIGN_TOP;
    request.layout.wrap = FW_TEXT_WRAP;
    request.layout.overflow = FW_TEXT_OVERFLOW_SCROLL;
    request.layout.padding = (fw_edge_insets_f32){8, 6, 8, 6};
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.max_width = 240.0f;
    request.constraints.max_height = 64.0f;
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.25f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.session.struct_size = sizeof(request.session);
    request.session.scroll_offset_y = 20.0f;
    return request;
}

int main(void) {
    const fw_plugin_api_v1 *plugin_api =
        facetwire_text_renderer_plugin_query(FW_ABI_VERSION_CURRENT);
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle plugin = NULL;
    const void *iface = NULL;
    const fw_text_renderer_api_v1 *renderer;
    fw_text_renderer_request_v1 request = make_request();
    fw_text_renderer_request_v1 request_copy;
    fw_string_view families_a[] = {FW_STRING_VIEW_LITERAL("Noto Sans")};
    fw_string_view families_b[] = {FW_STRING_VIEW_LITERAL("Noto Sans")};
    fw_text_render_result_v1 render_copy = {0};
    fw_text_validation_result_v1 validation = {0};
    fw_text_measure_result_v1 measure = {0};
    fw_text_render_result_v1 render = {0};
    fw_text_semantics_v1 semantics = {0};
    fake_state state = {0};
    fw_text_service_v2 text = {sizeof(text), &state, fake_layout, fake_release};
    fw_display_list_sink_v1 sink = {
        sizeof(sink), &state, fake_save, fake_restore, fake_clip,
        fake_fill, fake_stroke, fake_symbol, fake_draw};
    fw_text_services_v1 services = {sizeof(services), &sink, &text, 0u};
    fw_string_view schema = {0};
    request.style.font_families = families_a;
    request.style.font_family_count = 1u;
    CHECK(plugin_api != NULL);
    CHECK(plugin_api->load(&host, &plugin) == FW_STATUS_OK);
    CHECK(plugin_api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_TEXT_RENDERER_INTERFACE_ID),
        1u, &iface) == FW_STATUS_OK);
    renderer = (const fw_text_renderer_api_v1 *)iface;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) == FW_STATUS_OK);
    CHECK((validation.normalization_flags & FW_TX_NORMALIZED_NEWLINES) != 0u);
    measure.struct_size = sizeof(measure);
    CHECK(renderer->measure(plugin, &request, &services, &measure) == FW_STATUS_OK);
    CHECK(measure.line_count == 3u);
    CHECK(fabsf(state.last_request.font_size - 25.0f) < 0.001f);
    CHECK(state.last_request.text.length == request.text.length - 1u);
    CHECK(state.layouts == state.releases);
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request,
        (fw_rect_f32){0, 0, 240, 64}, &services, &render) == FW_STATUS_OK);
    CHECK(state.saves == 1u && state.restores == 1u && state.clips == 1u);
    CHECK(state.fills == 1u && state.draws == 1u);
    CHECK(fabsf(state.last_alpha - 0.4f) < 0.001f);
    CHECK(render.applied_scroll_offset_y > 0.0f);
    CHECK(state.layouts == state.releases);
    request_copy = request;
    request_copy.style.font_families = families_b;
    render_copy.struct_size = sizeof(render_copy);
    CHECK(renderer->render(plugin, &request_copy,
        (fw_rect_f32){0, 0, 240, 64}, &services, &render_copy) == FW_STATUS_OK);
    CHECK(render_copy.cache_key_high == render.cache_key_high);
    CHECK(render_copy.cache_key_low == render.cache_key_low);
    CHECK(state.layouts == state.releases);
    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(plugin, &request,
        (fw_rect_f32){0, 0, 240, 64}, &measure, &semantics) == FW_STATUS_OK);
    CHECK(semantics.selectable == 1u && semantics.scrollable == 1u);
    CHECK(semantics.text.length == request.text.length);
    CHECK(renderer->get_parameter_schema(plugin, &schema) == FW_STATUS_OK);
    CHECK(schema.length != 0u);
    request.opacity = 1.01f;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);
    plugin_api->unload(plugin);
    puts("text renderer contract passed");
    return 0;
}
