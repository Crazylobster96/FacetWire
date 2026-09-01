/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/hierarchical_chart_renderer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #value); \
    return 1; } } while (0)

typedef struct fake_sink {
    uint32_t begins, ends, rects, circles, sectors, labels, calls, fail_at;
    fw_point_f32 first_point;
    uint32_t has_point;
    uint32_t value_label_seen;
    float maximum_label_size;
} fake_sink;

static fw_status next(fake_sink *state) {
    ++state->calls;
    return state->fail_at != 0u && state->calls == state->fail_at ?
        FW_STATUS_PLUGIN_ERROR : FW_STATUS_OK;
}
static fw_status FW_CALL begin(void *data,
    const fw_visual_transform_result_v1 *transform, float opacity) {
    fake_sink *state = (fake_sink *)data; fw_status status = next(state);
    (void)transform; (void)opacity;
    if (status == FW_STATUS_OK) ++state->begins; return status;
}
static fw_status FW_CALL end(void *data) {
    fake_sink *state = (fake_sink *)data; fw_status status = next(state);
    if (status == FW_STATUS_OK) ++state->ends; return status;
}
static fw_status FW_CALL rect(void *data, fw_rect_f32 value,
    fw_color_rgba_f32 color, fw_string_view a, fw_string_view b) {
    fake_sink *state = (fake_sink *)data; fw_status status = next(state);
    (void)color; (void)a; (void)b;
    if (status == FW_STATUS_OK) {
        ++state->rects;
        if (state->has_point == 0u) {
            state->first_point = (fw_point_f32){
                value.x + value.width * 0.5f,
                value.y + value.height * 0.5f}; state->has_point = 1u;
        }
    }
    return status;
}
static fw_status FW_CALL line(void *data, fw_point_f32 a, fw_point_f32 b,
    float width, fw_color_rgba_f32 color, fw_string_view c, fw_string_view d) {
    (void)a; (void)b; (void)width; (void)color; (void)c; (void)d;
    return next((fake_sink *)data);
}
static fw_status FW_CALL circle(void *data, fw_point_f32 center,
    float radius, fw_color_rgba_f32 color, fw_string_view a, fw_string_view b) {
    fake_sink *state = (fake_sink *)data; fw_status status = next(state);
    (void)radius; (void)color; (void)a; (void)b;
    if (status == FW_STATUS_OK) {
        ++state->circles;
        if (state->has_point == 0u) {
            state->first_point = center; state->has_point = 1u;
        }
    }
    return status;
}
static fw_status FW_CALL sector(void *data, fw_point_f32 center,
    float outer, float inner, float start, float sweep,
    fw_color_rgba_f32 color, fw_string_view a, fw_string_view b) {
    fake_sink *state = (fake_sink *)data; fw_status status = next(state);
    (void)color; (void)a; (void)b;
    if (status == FW_STATUS_OK) {
        const float angle = start + sweep * 0.5f;
        const float radius = (outer + inner) * 0.5f;
        ++state->sectors;
        if (state->has_point == 0u) {
            state->first_point.x = center.x + cosf(angle) * radius;
            state->first_point.y = center.y + sinf(angle) * radius;
            state->has_point = 1u;
        }
    }
    return status;
}
static fw_status FW_CALL polygon(void *data, const fw_point_f32 *points,
    size_t count, fw_color_rgba_f32 color, fw_string_view a, fw_string_view b) {
    (void)points; (void)count; (void)color; (void)a; (void)b;
    return next((fake_sink *)data);
}
static fw_status FW_CALL label(void *data, fw_string_view text,
    fw_point_f32 anchor, float size, fw_color_rgba_f32 color,
    fw_string_view id) {
    fake_sink *state = (fake_sink *)data; fw_status status = next(state);
    (void)anchor; (void)color; (void)id;
    if (status == FW_STATUS_OK) {
        size_t index;
        ++state->labels;
        if (size > state->maximum_label_size)
            state->maximum_label_size = size;
        for (index = 0u; index + 1u < text.length; ++index)
            if (text.data[index] == '6' && text.data[index + 1u] == '0')
                state->value_label_seen = 1u;
    }
    return status;
}
static fw_chart_draw_sink_v1 make_sink(fake_sink *state) {
    fw_chart_draw_sink_v1 sink = {sizeof(sink), state, begin, end, rect,
        line, circle, sector, polygon, label};
    return sink;
}

typedef struct fixture {
    fw_hierarchical_chart_node_v1 nodes[7];
    fw_hierarchical_chart_request_v1 request;
} fixture;

static fixture make_fixture(fw_hierarchical_chart_kind kind) {
    static const char *ids[] = {"root", "a", "a1", "a2", "b", "b1", "b2"};
    static const char *labels[] = {"All", "Compute", "CPU", "GPU",
        "Storage", "SSD", "Archive"};
    static const uint32_t parents[] = {FW_HIERARCHICAL_ROOT_INDEX, 0u, 1u,
        1u, 0u, 4u, 4u};
    static const double values[] = {100.0, 60.0, 35.0, 25.0, 40.0, 24.0, 16.0};
    fixture result;
    size_t index;
    memset(&result, 0, sizeof(result));
    for (index = 0u; index < 7u; ++index) {
        result.nodes[index].struct_size = sizeof(result.nodes[index]);
        result.nodes[index].id.data = ids[index];
        result.nodes[index].id.length = strlen(ids[index]);
        result.nodes[index].label.data = labels[index];
        result.nodes[index].label.length = strlen(labels[index]);
        result.nodes[index].parent_index = parents[index];
        result.nodes[index].value = values[index];
        result.nodes[index].color = (fw_color_rgba_f32){
            0.16f + (float)index * 0.08f,
            0.38f + (float)(index % 3u) * 0.12f, 0.82f, 0.9f};
        result.nodes[index].visible = 1u;
    }
    result.request.struct_size = sizeof(result.request);
    result.request.request_id = 11u;
    result.request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:h");
    result.request.chart_id = (fw_string_view)FW_STRING_VIEW_LITERAL("chart:h");
    result.request.title = (fw_string_view)FW_STRING_VIEW_LITERAL("Resources");
    result.request.summary = (fw_string_view)FW_STRING_VIEW_LITERAL("Hierarchy");
    result.request.kind = kind;
    result.request.nodes = result.nodes;
    result.request.node_count = 7u;
    result.request.opacity = 0.72f;
    result.request.intrinsic_size = (fw_size_f32){640.0f, 420.0f};
    result.request.transform.struct_size = sizeof(result.request.transform);
    result.request.transform.fit = FW_VISUAL_FIT_CONTAIN;
    result.request.transform.alignment_x = 0.5f;
    result.request.transform.alignment_y = 0.5f;
    result.request.transform.clip = 1u;
    result.request.style.struct_size = sizeof(result.request.style);
    result.request.style.show_labels = 1u;
    result.request.style.show_values = 1u;
    result.request.style.max_visible_labels = 6u;
    result.request.style.gap = 0.006f;
    result.request.style.inner_radius = 0.28f;
    result.request.style.label_scale = 1.0f;
    result.request.budget.struct_size = sizeof(result.request.budget);
    result.request.constraints.struct_size = sizeof(result.request.constraints);
    result.request.target.struct_size = sizeof(result.request.target);
    result.request.target.device_pixel_ratio = 1.0f;
    result.request.target.font_scale = 1.0f;
    result.request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    result.request.target.supports_alpha = 1u;
    result.request.presentation_revision = 4u;
    return result;
}
static void rebind(fixture *value) { value->request.nodes = value->nodes; }

int main(void) {
    const fw_plugin_api_v1 *plugin_api;
    const fw_hierarchical_chart_api_v1 *api = NULL;
    fw_host_api_v1 host = {sizeof(host), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle handle = NULL;
    uint32_t kind;
    plugin_api = facetwire_hierarchical_chart_plugin_query(FW_ABI_VERSION_CURRENT);
    CHECK(plugin_api != NULL);
    CHECK(plugin_api->load(&host, &handle) == FW_STATUS_OK);
    CHECK(plugin_api->query_interface(handle,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_HIERARCHICAL_CHART_INTERFACE_ID),
        1u, (const void **)&api) == FW_STATUS_OK);
    CHECK(api != NULL);
    for (kind = FW_HIERARCHICAL_CHART_TREEMAP;
        kind <= FW_HIERARCHICAL_CHART_PACKED_BUBBLE; ++kind) {
        fixture value = make_fixture(kind);
        fake_sink state = {0};
        fw_chart_draw_sink_v1 draw = make_sink(&state);
        fw_chart_services_v1 services = {sizeof(services), &draw, 0u};
        fw_chart_validation_result_v1 validation = {sizeof(validation)};
        fw_chart_measure_result_v1 measure = {sizeof(measure)};
        fw_chart_render_result_v1 render = {sizeof(render)};
        fw_chart_semantics_v1 semantics = {sizeof(semantics)};
        fw_hierarchical_chart_hit_result_v1 hit = {sizeof(hit)};
        fw_point_f32 point;
        rebind(&value);
        CHECK(api->validate(handle, &value.request, &validation) == FW_STATUS_OK);
        CHECK(validation.status == FW_STATUS_OK);
        CHECK(api->measure(handle, &value.request, &measure) == FW_STATUS_OK);
        CHECK(measure.size.width == 640.0f && measure.size.height == 420.0f);
        CHECK(api->render(handle, &value.request,
            (fw_rect_f32){0.0f, 0.0f, 640.0f, 420.0f},
            &services, &render) == FW_STATUS_OK);
        CHECK(state.begins == 1u && state.ends == 1u && state.has_point == 1u);
        CHECK(render.rendered_value_count == 6u);
        CHECK(render.uncovered_is_transparent == 1u);
        CHECK(state.value_label_seen == 1u);
        CHECK(state.maximum_label_size > 0.0f &&
            state.maximum_label_size <= 0.112f);
        if (kind == FW_HIERARCHICAL_CHART_TREEMAP) CHECK(state.rects == 6u);
        if (kind == FW_HIERARCHICAL_CHART_SUNBURST) CHECK(state.sectors == 6u);
        if (kind == FW_HIERARCHICAL_CHART_PACKED_BUBBLE) CHECK(state.circles == 6u);
        CHECK(api->build_semantics(handle, &value.request,
            (fw_rect_f32){0.0f, 0.0f, 640.0f, 420.0f},
            &semantics) == FW_STATUS_OK);
        CHECK(semantics.role == FW_SEMANTICS_ROLE_CHART &&
            semantics.value_count == 6u);
        point.x = state.first_point.x * 640.0f;
        point.y = state.first_point.y * 420.0f;
        CHECK(api->hit_test(handle, &value.request,
            (fw_rect_f32){0.0f, 0.0f, 640.0f, 420.0f}, point,
            &hit) == FW_STATUS_OK);
        CHECK(hit.hit == 1u && hit.node_index > 0u);
    }
    {
        fixture value = make_fixture(FW_HIERARCHICAL_CHART_TREEMAP);
        fw_chart_validation_result_v1 validation = {sizeof(validation)};
        rebind(&value);
        value.nodes[1].visible = 0u;
        CHECK(api->validate(handle, &value.request, &validation) == FW_STATUS_OK);
        CHECK(validation.status == FW_STATUS_INVALID_ARGUMENT);
    }
    {
        fixture value = make_fixture(FW_HIERARCHICAL_CHART_TREEMAP);
        fw_chart_validation_result_v1 validation = {sizeof(validation)};
        rebind(&value);
        value.nodes[2].parent_index = 4u;
        CHECK(api->validate(handle, &value.request, &validation) == FW_STATUS_OK);
        CHECK(validation.status == FW_STATUS_INVALID_ARGUMENT);
    }
    {
        fixture value = make_fixture(FW_HIERARCHICAL_CHART_TREEMAP);
        fake_sink state = {0};
        fw_chart_draw_sink_v1 draw;
        fw_chart_services_v1 services;
        fw_chart_render_result_v1 render = {sizeof(render)};
        rebind(&value); state.fail_at = 3u; draw = make_sink(&state);
        services = (fw_chart_services_v1){sizeof(services), &draw, 0u};
        CHECK(api->render(handle, &value.request,
            (fw_rect_f32){0.0f, 0.0f, 640.0f, 420.0f},
            &services, &render) == FW_STATUS_SINK_REJECTED);
        CHECK(state.begins == 1u && state.ends == 1u);
    }
    plugin_api->unload(handle);
    puts("hierarchical chart renderer tests passed");
    return 0;
}
