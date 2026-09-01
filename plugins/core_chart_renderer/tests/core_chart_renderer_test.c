/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/chart_element_layer.h>
#include <facetwire/chart_legend.h>
#include <facetwire/chart_presentation.h>
#include <facetwire/chart_renderer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #value); \
    return 1; } } while (0)

typedef struct fake_sink {
    uint32_t begins;
    uint32_t ends;
    uint32_t rects;
    uint32_t lines;
    uint32_t circles;
    uint32_t sectors;
    uint32_t polygons;
    uint32_t labels;
    uint32_t data_rects;
    uint32_t calls;
    uint32_t fail_at;
    float opacity;
    fw_rect_f32 first_rect;
    fw_rect_f32 sales_q2_rect;
    fw_color_rgba_f32 sales_q2_color;
    uint32_t sales_q2_rects;
    fw_point_f32 category_label_anchors[3];
    uint32_t category_label_count;
    fw_point_f32 direct_sales_anchor;
    uint32_t direct_sales_count;
    fw_visual_transform_result_v1 transform;
} fake_sink;

static fw_status fake_status(fake_sink *state) {
    ++state->calls;
    return state->fail_at != 0u && state->calls == state->fail_at ?
        FW_STATUS_PLUGIN_ERROR : FW_STATUS_OK;
}

static fw_status FW_CALL fake_begin(void *user_data,
    const fw_visual_transform_result_v1 *transform, float opacity) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    if (status == FW_STATUS_OK) {
        ++state->begins;
        state->transform = *transform;
        state->opacity = opacity;
    }
    return status;
}

static fw_status FW_CALL fake_end(void *user_data) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    if (status == FW_STATUS_OK) ++state->ends;
    return status;
}

static fw_status FW_CALL fake_rect(void *user_data, fw_rect_f32 rect,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    if (status == FW_STATUS_OK) {
        if (category_id.length != 0u) {
            if (state->data_rects == 0u) state->first_rect = rect;
            ++state->data_rects;
        }
        if (series_id.length == 5u && category_id.length == 2u &&
            memcmp(series_id.data, "sales", 5u) == 0 &&
            memcmp(category_id.data, "q2", 2u) == 0) {
            state->sales_q2_rect = rect;
            state->sales_q2_color = color;
            ++state->sales_q2_rects;
        }
        ++state->rects;
    }
    return status;
}

static fw_status FW_CALL fake_line(void *user_data, fw_point_f32 start,
    fw_point_f32 end, float width, fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    (void)start; (void)end; (void)width; (void)color;
    (void)series_id; (void)category_id;
    if (status == FW_STATUS_OK) ++state->lines;
    return status;
}

static fw_status FW_CALL fake_circle(void *user_data, fw_point_f32 center,
    float radius, fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    (void)center; (void)radius; (void)color;
    (void)series_id; (void)category_id;
    if (status == FW_STATUS_OK) ++state->circles;
    return status;
}

static fw_status FW_CALL fake_sector(void *user_data, fw_point_f32 center,
    float outer_radius, float inner_radius, float start, float sweep,
    fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    (void)center; (void)outer_radius; (void)inner_radius;
    (void)start; (void)sweep; (void)color;
    (void)series_id; (void)category_id;
    if (status == FW_STATUS_OK) ++state->sectors;
    return status;
}

static fw_status FW_CALL fake_polygon(void *user_data,
    const fw_point_f32 *points, size_t point_count,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    (void)points; (void)color; (void)series_id; (void)category_id;
    if (status == FW_STATUS_OK && point_count >= 3u) ++state->polygons;
    return status;
}

static fw_status FW_CALL fake_label(void *user_data, fw_string_view text,
    fw_point_f32 anchor, float font_size, fw_color_rgba_f32 color,
    fw_string_view element_id) {
    fake_sink *state = (fake_sink *)user_data;
    const fw_status status = fake_status(state);
    (void)text; (void)font_size; (void)color;
    if (status == FW_STATUS_OK) {
        if (element_id.length == 2u && element_id.data != NULL &&
            element_id.data[0] == 'q' && element_id.data[1] >= '1' &&
            element_id.data[1] <= '3') {
            const uint32_t index = (uint32_t)(element_id.data[1] - '1');
            state->category_label_anchors[index] = anchor;
            ++state->category_label_count;
        }
        if (text.length == 5u && element_id.length == 5u &&
            memcmp(text.data, "Sales", 5u) == 0 &&
            memcmp(element_id.data, "sales", 5u) == 0) {
            state->direct_sales_anchor = anchor;
            ++state->direct_sales_count;
        }
        ++state->labels;
    }
    return status;
}

typedef struct chart_fixture {
    fw_chart_category_v1 categories[3];
    fw_chart_value_v1 values_a[3];
    fw_chart_value_v1 values_b[3];
    fw_chart_series_v1 series[2];
    fw_chart_renderer_request_v1 request;
} chart_fixture;

static fw_color_rgba_f32 color(float red, float green, float blue) {
    fw_color_rgba_f32 result = {red, green, blue, 1.0f};
    return result;
}

static chart_fixture make_fixture(fw_chart_kind kind) {
    chart_fixture fixture;
    size_t i;
    static const char *ids[] = {"q1", "q2", "q3"};
    static const char *labels[] = {"Q1", "Q2", "Q3"};
    memset(&fixture, 0, sizeof(fixture));
    for (i = 0u; i < 3u; ++i) {
        fixture.categories[i].struct_size = sizeof(fixture.categories[i]);
        fixture.categories[i].id.data = ids[i];
        fixture.categories[i].id.length = 2u;
        fixture.categories[i].label.data = labels[i];
        fixture.categories[i].label.length = 2u;
        fixture.values_a[i].struct_size = sizeof(fixture.values_a[i]);
        fixture.values_a[i].value = (double)(i + 1u) * 10.0;
        fixture.values_b[i].struct_size = sizeof(fixture.values_b[i]);
        fixture.values_b[i].value = (double)(i + 1u) * 5.0;
    }
    fixture.series[0].struct_size = sizeof(fixture.series[0]);
    fixture.series[0].id = (fw_string_view)FW_STRING_VIEW_LITERAL("sales");
    fixture.series[0].label = (fw_string_view)FW_STRING_VIEW_LITERAL("Sales");
    fixture.series[0].values = fixture.values_a;
    fixture.series[0].value_count = 3u;
    fixture.series[0].color = color(0.2f, 0.45f, 0.9f);
    fixture.series[0].visible = 1u;
    fixture.series[1].struct_size = sizeof(fixture.series[1]);
    fixture.series[1].id = (fw_string_view)FW_STRING_VIEW_LITERAL("cost");
    fixture.series[1].label = (fw_string_view)FW_STRING_VIEW_LITERAL("Cost");
    fixture.series[1].values = fixture.values_b;
    fixture.series[1].value_count = 3u;
    fixture.series[1].color = color(0.9f, 0.35f, 0.25f);
    fixture.series[1].visible = 1u;
    fixture.request.struct_size = sizeof(fixture.request);
    fixture.request.request_id = 7u;
    fixture.request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:chart");
    fixture.request.chart_id = (fw_string_view)FW_STRING_VIEW_LITERAL("chart.sales");
    fixture.request.title = (fw_string_view)FW_STRING_VIEW_LITERAL("Quarterly sales");
    fixture.request.summary = (fw_string_view)FW_STRING_VIEW_LITERAL("Sales rise each quarter");
    fixture.request.kind = kind;
    fixture.request.categories = fixture.categories;
    fixture.request.category_count = 3u;
    fixture.request.series = fixture.series;
    fixture.request.series_count = kind == FW_CHART_PIE ? 1u : 2u;
    fixture.request.opacity = 0.65f;
    fixture.request.intrinsic_size = (fw_size_f32){640.0f, 360.0f};
    fixture.request.transform.struct_size = sizeof(fixture.request.transform);
    fixture.request.transform.fit = FW_VISUAL_FIT_CONTAIN;
    fixture.request.transform.alignment_x = 0.5f;
    fixture.request.transform.alignment_y = 0.5f;
    fixture.request.transform.clip = 1u;
    fixture.request.style.struct_size = sizeof(fixture.request.style);
    fixture.request.style.show_axes = 1u;
    fixture.request.style.show_grid = 1u;
    fixture.request.style.show_legend = 1u;
    fixture.request.style.show_labels = 1u;
    fixture.request.style.bar_gap_ratio = 0.2f;
    fixture.request.style.line_width = 0.006f;
    fixture.request.style.point_radius = 0.012f;
    fixture.request.style.foreground = color(0.1f, 0.1f, 0.15f);
    fixture.request.style.grid_color = color(0.8f, 0.82f, 0.86f);
    fixture.request.budget.struct_size = sizeof(fixture.request.budget);
    fixture.request.constraints.struct_size = sizeof(fixture.request.constraints);
    fixture.request.constraints.min_width = 100.0f;
    fixture.request.constraints.max_width = 640.0f;
    fixture.request.constraints.min_height = 80.0f;
    fixture.request.constraints.max_height = 360.0f;
    fixture.request.target.struct_size = sizeof(fixture.request.target);
    fixture.request.target.device_pixel_ratio = 1.0f;
    fixture.request.target.font_scale = 1.0f;
    fixture.request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    fixture.request.target.supports_alpha = 1u;
    fixture.request.presentation_revision = 3u;
    return fixture;
}

static void rebind_fixture(chart_fixture *fixture) {
    fixture->series[0].values = fixture->values_a;
    fixture->series[1].values = fixture->values_b;
    fixture->request.categories = fixture->categories;
    fixture->request.series = fixture->series;
}

static fw_chart_draw_sink_v1 make_sink(fake_sink *state) {
    fw_chart_draw_sink_v1 sink = {sizeof(sink), state, fake_begin, fake_end,
        fake_rect, fake_line, fake_circle, fake_sector, fake_polygon,
        fake_label};
    return sink;
}

typedef struct element_test_state {
    const fw_chart_element_api_v1 *api;
    fw_plugin_handle plugin;
    uint32_t count;
    uint32_t roles[FW_CHART_ELEMENT_ROLE_MAX + 1u];
    char ids[64][160];
    uint32_t id_count;
    fw_chart_element_ref_v1 sales_q2;
} element_test_state;

static fw_status FW_CALL visit_element(void *user_data,
    const fw_chart_element_descriptor_v1 *descriptor) {
    element_test_state *state = (element_test_state *)user_data;
    size_t required = 0u;
    uint32_t i;
    if (descriptor == NULL || descriptor->struct_size < sizeof(*descriptor) ||
        descriptor->ref.role > FW_CHART_ELEMENT_ROLE_MAX ||
        state->id_count >= 64u) return FW_STATUS_INVALID_ARGUMENT;
    if (descriptor->ref.role <= FW_CHART_ELEMENT_ROLE_MAX)
        ++state->roles[descriptor->ref.role];
    if (descriptor->ref.role == FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER &&
        descriptor->parent.role != FW_CHART_ELEMENT_ROLE_CHART_ROOT)
        return FW_STATUS_INVALID_STATE;
    if (descriptor->ref.role == FW_CHART_ELEMENT_ROLE_LEGEND_ITEM &&
        descriptor->parent.role != FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER)
        return FW_STATUS_INVALID_STATE;
    if ((descriptor->ref.role == FW_CHART_ELEMENT_ROLE_LEGEND_MARKER ||
         descriptor->ref.role == FW_CHART_ELEMENT_ROLE_LEGEND_LABEL ||
         descriptor->ref.role == FW_CHART_ELEMENT_ROLE_LEGEND_VALUE) &&
        descriptor->parent.role != FW_CHART_ELEMENT_ROLE_LEGEND_ITEM)
        return FW_STATUS_INVALID_STATE;
    if (state->api->format_element_id(state->plugin, &descriptor->ref,
        NULL, 0u, &required) != FW_STATUS_BUFFER_TOO_SMALL ||
        required + 1u > sizeof(state->ids[0]))
        return FW_STATUS_INVALID_STATE;
    if (state->api->format_element_id(state->plugin, &descriptor->ref,
        state->ids[state->id_count], sizeof(state->ids[0]),
        &required) != FW_STATUS_OK) return FW_STATUS_INVALID_STATE;
    for (i = 0u; i < state->id_count; ++i)
        if (strcmp(state->ids[i], state->ids[state->id_count]) == 0)
            return FW_STATUS_ALREADY_REGISTERED;
    if (descriptor->ref.role == FW_CHART_ELEMENT_ROLE_DATUM &&
        descriptor->ref.series_id.length == 5u &&
        descriptor->ref.category_id.length == 2u &&
        memcmp(descriptor->ref.series_id.data, "sales", 5u) == 0 &&
        memcmp(descriptor->ref.category_id.data, "q2", 2u) == 0)
        state->sales_q2 = descriptor->ref;
    ++state->id_count;
    ++state->count;
    return FW_STATUS_OK;
}

typedef struct observer_test_state {
    uint32_t calls;
    uint32_t sales_q2_calls;
    uint32_t sales_legend_marker_calls;
    uint32_t sales_legend_label_calls;
    fw_chart_element_presentation_v1 sales_q2;
    fw_chart_element_presentation_v1 sales_legend_marker;
    fw_chart_element_presentation_v1 sales_legend_label;
} observer_test_state;

static fw_status FW_CALL observe_element(void *user_data,
    const fw_chart_element_descriptor_v1 *descriptor,
    const fw_chart_element_presentation_v1 *presentation) {
    observer_test_state *state = (observer_test_state *)user_data;
    if (descriptor == NULL || presentation == NULL ||
        descriptor->struct_size < sizeof(*descriptor) ||
        presentation->struct_size < sizeof(*presentation))
        return FW_STATUS_INVALID_ARGUMENT;
    ++state->calls;
    if (descriptor->ref.series_id.length == 5u &&
        descriptor->ref.category_id.length == 2u &&
        memcmp(descriptor->ref.series_id.data, "sales", 5u) == 0 &&
        memcmp(descriptor->ref.category_id.data, "q2", 2u) == 0) {
        state->sales_q2 = *presentation;
        ++state->sales_q2_calls;
    }
    if (descriptor->ref.series_id.length == 5u &&
        memcmp(descriptor->ref.series_id.data, "sales", 5u) == 0) {
        if (descriptor->ref.role == FW_CHART_ELEMENT_ROLE_LEGEND_MARKER) {
            state->sales_legend_marker = *presentation;
            ++state->sales_legend_marker_calls;
        } else if (descriptor->ref.role ==
            FW_CHART_ELEMENT_ROLE_LEGEND_LABEL) {
            state->sales_legend_label = *presentation;
            ++state->sales_legend_label_calls;
        }
    }
    return FW_STATUS_OK;
}

int main(void) {
    const fw_plugin_api_v1 *plugin_api =
        facetwire_core_chart_plugin_query(FW_ABI_VERSION_CURRENT);
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle plugin = NULL;
    const void *interface_value = NULL;
    const fw_chart_renderer_api_v1 *renderer;
    const fw_chart_element_api_v1 *elements;
    const fw_chart_presentation_api_v1 *presentation_api;
    chart_fixture fixture = make_fixture(FW_CHART_BAR);
    fake_sink state = {0};
    fw_chart_draw_sink_v1 sink = make_sink(&state);
    fw_chart_services_v1 services = {sizeof(services), &sink, 0u};
    fw_chart_validation_result_v1 validation = {0};
    fw_chart_measure_result_v1 measure = {0};
    fw_chart_render_result_v1 render = {0};
    fw_chart_semantics_v1 semantics = {0};
    fw_chart_hit_result_v1 hit = {0};
    fw_string_view schema = {0};
    fw_rect_f32 bounds = {0.0f, 0.0f, 640.0f, 360.0f};
    rebind_fixture(&fixture);
    CHECK(plugin_api != NULL);
    CHECK(plugin_api->load(&host, &plugin) == FW_STATUS_OK);
    CHECK(plugin_api->get_descriptor()->capability_count == 1u);
    CHECK(plugin_api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_CHART_RENDERER_INTERFACE_ID),
        1u, &interface_value) == FW_STATUS_OK);
    renderer = (const fw_chart_renderer_api_v1 *)interface_value;
    interface_value = NULL;
    CHECK(plugin_api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_CHART_ELEMENT_INTERFACE_ID),
        1u, &interface_value) == FW_STATUS_OK);
    elements = (const fw_chart_element_api_v1 *)interface_value;
    interface_value = NULL;
    CHECK(plugin_api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(
            FW_CHART_PRESENTATION_INTERFACE_ID),
        1u, &interface_value) == FW_STATUS_OK);
    presentation_api = (const fw_chart_presentation_api_v1 *)interface_value;
    CHECK(renderer->get_parameter_schema(plugin, &schema) == FW_STATUS_OK);
    CHECK(schema.length != 0u);

    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &fixture.request, &validation) ==
        FW_STATUS_OK);
    CHECK(validation.status == FW_STATUS_OK);
    measure.struct_size = sizeof(measure);
    CHECK(renderer->measure(plugin, &fixture.request, &measure) ==
        FW_STATUS_OK);
    CHECK(fabsf(measure.size.width - 640.0f) < 0.001f);
    CHECK(fabsf(measure.size.height - 360.0f) < 0.001f);

    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    CHECK(state.begins == 1u && state.ends == 1u);
    CHECK(state.rects == 8u && state.lines == 7u);
    CHECK(state.labels == 6u);
    CHECK(fabsf(state.opacity - 0.65f) < 0.001f);
    CHECK(render.rendered_series_count == 2u);
    CHECK(render.rendered_value_count == 6u);
    CHECK(render.uncovered_is_transparent == 1u);
    CHECK(render.cache_key_high != 0u && render.cache_key_low != 0u);

    {
        element_test_state element_state;
        observer_test_state observer_state;
        fw_chart_element_enum_sink_v1 enum_sink;
        fw_chart_element_enum_result_v1 enum_result;
        fw_chart_element_override_v1 overrides[3];
        fw_chart_element_observer_v1 observer;
        fw_chart_validation_result_v1 override_validation;
        uint64_t base_high = render.cache_key_high;
        uint64_t base_low = render.cache_key_low;
        memset(&element_state, 0, sizeof(element_state));
        element_state.api = elements;
        element_state.plugin = plugin;
        enum_sink = (fw_chart_element_enum_sink_v1){
            sizeof(enum_sink), &element_state, visit_element};
        memset(&enum_result, 0, sizeof(enum_result));
        enum_result.struct_size = sizeof(enum_result);
        CHECK(elements->enumerate(plugin, &fixture.request, &enum_sink,
            &enum_result) == FW_STATUS_OK);
        CHECK(enum_result.emitted_element_count == 24u);
        CHECK(element_state.roles[FW_CHART_ELEMENT_ROLE_CHART_ROOT] == 1u);
        CHECK(element_state.roles[FW_CHART_ELEMENT_ROLE_SERIES] == 2u);
        CHECK(element_state.roles[FW_CHART_ELEMENT_ROLE_DATUM] == 6u);
        CHECK(element_state.roles[
            FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER] == 1u);
        CHECK(element_state.roles[FW_CHART_ELEMENT_ROLE_LEGEND_ITEM] == 2u);
        CHECK(element_state.roles[FW_CHART_ELEMENT_ROLE_LEGEND_MARKER] == 2u);
        CHECK(element_state.roles[FW_CHART_ELEMENT_ROLE_LEGEND_LABEL] == 2u);
        CHECK(element_state.sales_q2.role == FW_CHART_ELEMENT_ROLE_DATUM);
        CHECK(strstr(element_state.ids[0], "chart/chart.sales/") != NULL);

        memset(overrides, 0, sizeof(overrides));
        overrides[0].struct_size = sizeof(overrides[0]);
        overrides[0].selector.struct_size = sizeof(overrides[0].selector);
        overrides[0].selector.role = FW_CHART_ELEMENT_ROLE_SERIES;
        overrides[0].selector.chart_id = fixture.request.chart_id;
        overrides[0].selector.series_id = fixture.series[0].id;
        overrides[0].selector.part_index = FW_CHART_ELEMENT_PART_ANY;
        overrides[0].fields = FW_CHART_OVERRIDE_OPACITY;
        overrides[0].opacity = 0.5f;
        overrides[1].struct_size = sizeof(overrides[1]);
        overrides[1].selector = element_state.sales_q2;
        overrides[1].selector.part_index = FW_CHART_ELEMENT_PART_ANY;
        overrides[1].fields = FW_CHART_OVERRIDE_OPACITY |
            FW_CHART_OVERRIDE_COLOR | FW_CHART_OVERRIDE_TRANSLATION |
            FW_CHART_OVERRIDE_SCALE | FW_CHART_OVERRIDE_Z_OFFSET |
            FW_CHART_OVERRIDE_PROMOTION;
        overrides[1].opacity = 0.8f;
        overrides[1].color = color(0.1f, 0.8f, 0.35f);
        overrides[1].translation = (fw_point_f32){0.04f, -0.02f};
        overrides[1].uniform_scale = 1.1f;
        overrides[1].z_offset = 7;
        overrides[1].promotion = FW_CHART_ELEMENT_PROMOTED;
        overrides[2].struct_size = sizeof(overrides[2]);
        overrides[2].selector.struct_size = sizeof(overrides[2].selector);
        overrides[2].selector.role = FW_CHART_ELEMENT_ROLE_LEGEND_ITEM;
        overrides[2].selector.chart_id = fixture.request.chart_id;
        overrides[2].selector.series_id = fixture.series[0].id;
        overrides[2].selector.part_index = FW_CHART_ELEMENT_PART_ANY;
        overrides[2].fields = FW_CHART_OVERRIDE_OPACITY |
            FW_CHART_OVERRIDE_TRANSLATION | FW_CHART_OVERRIDE_SCALE |
            FW_CHART_OVERRIDE_ROTATION;
        overrides[2].opacity = 0.35f;
        overrides[2].translation = (fw_point_f32){0.02f, -0.01f};
        overrides[2].uniform_scale = 1.05f;
        overrides[2].rotation_radians = 0.10f;
        memset(&override_validation, 0, sizeof(override_validation));
        override_validation.struct_size = sizeof(override_validation);
        CHECK(elements->validate_overrides(plugin, &fixture.request,
            overrides, 3u, &override_validation) == FW_STATUS_OK);
        memset(&observer_state, 0, sizeof(observer_state));
        observer = (fw_chart_element_observer_v1){
            sizeof(observer), &observer_state, observe_element};
        memset(&state, 0, sizeof(state));
        sink = make_sink(&state);
        services.draw = &sink;
        render.struct_size = sizeof(render);
        CHECK(elements->render(plugin, &fixture.request, overrides, 3u,
            bounds, &services, &observer, &render) == FW_STATUS_OK);
        CHECK(observer_state.calls != 0u &&
            observer_state.sales_q2_calls != 0u);
        CHECK(fabsf(observer_state.sales_q2.opacity - 0.8f) < 0.001f);
        CHECK(observer_state.sales_q2.promotion ==
            FW_CHART_ELEMENT_PROMOTED);
        CHECK(observer_state.sales_q2.z_offset == 7);
        CHECK(state.sales_q2_rects == 1u);
        CHECK(fabsf(state.sales_q2_color.alpha - 0.8f) < 0.001f);
        CHECK(fabsf(state.sales_q2_color.green - 0.8f) < 0.001f);
        CHECK(observer_state.sales_legend_marker_calls == 1u);
        CHECK(observer_state.sales_legend_label_calls == 1u);
        CHECK(fabsf(observer_state.sales_legend_marker.opacity - 0.35f) <
            0.001f);
        CHECK(fabsf(observer_state.sales_legend_label.opacity - 0.35f) <
            0.001f);
        CHECK(fabsf(observer_state.sales_legend_marker.anchor.x -
            observer_state.sales_legend_label.anchor.x) < 0.001f);
        CHECK(fabsf(observer_state.sales_legend_marker.anchor.y -
            observer_state.sales_legend_label.anchor.y) < 0.001f);
        CHECK(render.cache_key_high != base_high ||
            render.cache_key_low != base_low);

        overrides[2].fields = FW_CHART_OVERRIDE_OPACITY;
        overrides[1].fields = FW_CHART_OVERRIDE_ROTATION |
            FW_CHART_OVERRIDE_ANCHOR;
        overrides[1].rotation_radians = 0.25f;
        overrides[1].anchor = (fw_point_f32){0.5f, 0.5f};
        memset(&observer_state, 0, sizeof(observer_state));
        memset(&state, 0, sizeof(state));
        sink = make_sink(&state);
        services.draw = &sink;
        render.struct_size = sizeof(render);
        CHECK(elements->render(plugin, &fixture.request, overrides, 3u,
            bounds, &services, &observer, &render) == FW_STATUS_OK);
        CHECK(state.sales_q2_rects == 0u);
        CHECK(state.polygons == 1u);
        CHECK(observer_state.sales_q2_calls != 0u);
        CHECK(fabsf(observer_state.sales_q2.rotation_radians - 0.25f) <
            0.001f);
        CHECK(fabsf(observer_state.sales_q2.anchor.x - 0.5f) < 0.001f);
        CHECK(fabsf(observer_state.sales_q2.anchor.y - 0.5f) < 0.001f);

        overrides[1].fields = FW_CHART_OVERRIDE_VISIBLE;
        overrides[1].visible = 0u;
        memset(&state, 0, sizeof(state));
        sink = make_sink(&state);
        services.draw = &sink;
        render.struct_size = sizeof(render);
        CHECK(elements->render(plugin, &fixture.request, overrides, 3u,
            bounds, &services, NULL, &render) == FW_STATUS_OK);
        CHECK(state.sales_q2_rects == 0u);

        overrides[1].fields = FW_CHART_OVERRIDE_SCALE;
        overrides[1].uniform_scale = 0.0f;
        override_validation.struct_size = sizeof(override_validation);
        CHECK(elements->validate_overrides(plugin, &fixture.request,
            overrides, 3u, &override_validation) ==
            FW_STATUS_INVALID_ARGUMENT);
    }

    fixture.request.style.show_value_labels = 1u;
    memset(&state, 0, sizeof(state));
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    CHECK(state.labels == 12u);

    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(plugin, &fixture.request, bounds,
        &semantics) == FW_STATUS_OK);
    CHECK(semantics.role == FW_SEMANTICS_ROLE_CHART);
    CHECK(semantics.series_count == 2u && semantics.value_count == 6u);

    hit.struct_size = sizeof(hit);
    CHECK(renderer->hit_test(plugin, &fixture.request, bounds,
        (fw_point_f32){
            (state.first_rect.x + state.first_rect.width * 0.5f) * 640.0f,
            (state.first_rect.y + state.first_rect.height * 0.5f) * 360.0f},
        &hit) == FW_STATUS_OK);
    CHECK(hit.hit == 1u && hit.element_kind == FW_CHART_ELEMENT_BAR);
    CHECK(hit.series_index == 0u && hit.value_index == 0u);
    CHECK(fabs(hit.value - 10.0) < 0.001);

    fixture = make_fixture(FW_CHART_LINE);
    rebind_fixture(&fixture);
    memset(&state, 0, sizeof(state));
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    CHECK(state.circles == 6u && state.lines == 11u);
    hit.struct_size = sizeof(hit);
    CHECK(renderer->hit_test(plugin, &fixture.request, bounds,
        (fw_point_f32){0.25f * 640.0f, 0.5866667f * 360.0f},
        &hit) == FW_STATUS_OK);
    CHECK(hit.hit == 1u &&
        hit.element_kind == FW_CHART_ELEMENT_LINE_POINT);

    fixture = make_fixture(FW_CHART_PIE);
    rebind_fixture(&fixture);
    memset(&state, 0, sizeof(state));
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    CHECK(state.sectors == 3u && state.rects == 1u);
    CHECK(state.category_label_count == 3u);
    {
        uint32_t i;
        for (i = 0u; i < 3u; ++i) {
            const float dx = state.category_label_anchors[i].x - 0.5f;
            const float dy = state.category_label_anchors[i].y - 0.46f;
            const float distance = sqrtf(dx * dx + dy * dy);
            CHECK(fabsf(distance - 0.22f) < 0.001f);
            CHECK(distance < 0.34f);
        }
    }
    hit.struct_size = sizeof(hit);
    CHECK(renderer->hit_test(plugin, &fixture.request, bounds,
        (fw_point_f32){0.58f * 640.0f, 0.30f * 360.0f},
        &hit) == FW_STATUS_OK);
    CHECK(hit.hit == 1u && hit.element_kind == FW_CHART_ELEMENT_PIE_SLICE);

    {
        fw_chart_kind advanced_kind;
        for (advanced_kind = FW_CHART_AREA;
            advanced_kind <= FW_CHART_COMBO; ++advanced_kind) {
            size_t value_index;
            fixture = make_fixture(advanced_kind);
            rebind_fixture(&fixture);
            if (advanced_kind == FW_CHART_DONUT ||
                advanced_kind == FW_CHART_GAUGE ||
                advanced_kind == FW_CHART_BOX_PLOT ||
                advanced_kind == FW_CHART_HISTOGRAM ||
                advanced_kind == FW_CHART_WATERFALL ||
                advanced_kind == FW_CHART_FUNNEL ||
                advanced_kind == FW_CHART_CANDLESTICK)
                fixture.request.series_count = 1u;
            fixture.series[0].mark = FW_CHART_MARK_BAR;
            fixture.series[1].mark = FW_CHART_MARK_LINE;
            fixture.request.style.show_value_labels = 1u;
            fixture.request.style.fill_opacity = 0.28f;
            fixture.request.style.donut_inner_radius = 0.56f;
            for (value_index = 0u; value_index < 3u; ++value_index) {
                fw_chart_value_v1 *value = &fixture.values_a[value_index];
                value->x = (double)value_index + 1.0;
                value->size = value->value;
                value->minimum = value->value - 4.0;
                value->quartile1 = value->value - 2.0;
                value->median = value->value;
                value->quartile3 = value->value + 2.0;
                value->maximum = value->value + 4.0;
                value->open = value->value - 1.0;
                value->high = value->value + 3.0;
                value->low = value->value - 3.0;
                value->close = value->value +
                    (value_index % 2u == 0u ? 2.0 : -2.0);
                fixture.values_b[value_index].x =
                    (double)value_index + 1.0;
                fixture.values_b[value_index].size =
                    fixture.values_b[value_index].value;
            }
            memset(&state, 0, sizeof(state));
            sink = make_sink(&state);
            services.draw = &sink;
            render.struct_size = sizeof(render);
            CHECK(renderer->render(plugin, &fixture.request, bounds,
                &services, &render) == FW_STATUS_OK);
            CHECK(state.begins == 1u && state.ends == 1u);
            CHECK(render.emitted_command_count > 2u);
            if (advanced_kind == FW_CHART_AREA ||
                advanced_kind == FW_CHART_RADAR ||
                advanced_kind == FW_CHART_FUNNEL)
                CHECK(state.polygons != 0u);
            if (advanced_kind == FW_CHART_DONUT ||
                advanced_kind == FW_CHART_GAUGE)
                CHECK(state.sectors != 0u);
        }
    }

    {
        fw_chart_kind expanded_kind;
        for (expanded_kind = FW_CHART_DIVERGING_BAR;
            expanded_kind <= FW_CHART_ROSE; ++expanded_kind) {
            size_t value_index;
            fixture = make_fixture(expanded_kind);
            rebind_fixture(&fixture);
            if (expanded_kind == FW_CHART_WORD_CLOUD ||
                expanded_kind == FW_CHART_ROSE)
                fixture.request.series_count = 1u;
            fixture.request.style.show_value_labels = 1u;
            fixture.request.style.fill_opacity = 0.30f;
            for (value_index = 0u; value_index < 3u; ++value_index) {
                fixture.values_a[value_index].x = (double)value_index + 1.0;
                fixture.values_a[value_index].size =
                    (double)value_index + 1.0;
                fixture.values_a[value_index].minimum =
                    fixture.values_a[value_index].value - 4.0;
                fixture.values_a[value_index].maximum =
                    fixture.values_a[value_index].value + 4.0;
                fixture.values_b[value_index].x = (double)value_index + 1.2;
                fixture.values_b[value_index].size =
                    (double)value_index + 1.0;
                fixture.values_b[value_index].minimum =
                    fixture.values_b[value_index].value - 2.0;
                fixture.values_b[value_index].maximum =
                    fixture.values_b[value_index].value + 2.0;
            }
            if (expanded_kind == FW_CHART_DIVERGING_BAR) {
                fixture.values_a[0].value = -18.0;
                fixture.values_a[1].value = -4.0;
            }
            memset(&state, 0, sizeof(state));
            sink = make_sink(&state);
            services.draw = &sink;
            render.struct_size = sizeof(render);
            CHECK(renderer->render(plugin, &fixture.request, bounds,
                &services, &render) == FW_STATUS_OK);
            CHECK(state.begins == 1u && state.ends == 1u);
            CHECK(render.uncovered_is_transparent == 1u);
            if (expanded_kind == FW_CHART_DIVERGING_BAR)
                CHECK(state.rects >= 6u);
            if (expanded_kind == FW_CHART_FACET_LINE)
                CHECK(state.circles == 6u && state.lines != 0u);
            if (expanded_kind == FW_CHART_RANGE_AREA)
                CHECK(state.polygons == 4u);
            if (expanded_kind == FW_CHART_DENSITY_HEATMAP)
                CHECK(state.rects == 98u);
            if (expanded_kind == FW_CHART_WORD_CLOUD)
                CHECK(state.labels >= 3u);
            if (expanded_kind == FW_CHART_ROSE)
                CHECK(state.sectors == 3u);
        }
    }

    {
        fw_chart_presentation_v1 presentation;
        fw_chart_presentation_plan_v1 plan;
        fw_chart_validation_result_v1 presentation_validation;
        fixture = make_fixture(FW_CHART_BAR);
        rebind_fixture(&fixture);
        memset(&presentation, 0, sizeof(presentation));
        presentation.struct_size = sizeof(presentation);
        presentation.theme = FW_CHART_THEME_BUSINESS;
        presentation.legend_placement = FW_CHART_LEGEND_AUTO;
        presentation.label_policy = FW_CHART_LABEL_AUTO;
        presentation.auto_layout = 1u;
        presentation.max_visible_labels = 4u;
        presentation.label_padding = 0.004f;
        presentation.title_scale = 1.18f;
        presentation.label_scale = 0.92f;
        presentation.value_scale = 0.88f;
        presentation.flags = FW_CHART_PRESENTATION_USE_THEME_PALETTE |
            FW_CHART_PRESENTATION_AVOID_COLLISIONS;
        memset(&presentation_validation, 0, sizeof(presentation_validation));
        presentation_validation.struct_size = sizeof(presentation_validation);
        CHECK(presentation_api->validate(plugin, &fixture.request,
            &presentation, &presentation_validation) == FW_STATUS_OK);
        CHECK(presentation_validation.status == FW_STATUS_OK);
        memset(&plan, 0, sizeof(plan));
        plan.struct_size = sizeof(plan);
        CHECK(presentation_api->resolve(plugin, &fixture.request,
            &presentation, &plan) == FW_STATUS_OK);
        CHECK(plan.resolved_theme == FW_CHART_THEME_BUSINESS);
        CHECK(plan.resolved_legend_placement == FW_CHART_LEGEND_BOTTOM);
        memset(&state, 0, sizeof(state));
        sink = make_sink(&state);
        services.draw = &sink;
        render.struct_size = sizeof(render);
        CHECK(presentation_api->render(plugin, &fixture.request,
            &presentation, NULL, 0u, bounds, &services, NULL,
            &render) == FW_STATUS_OK);
        CHECK(state.begins == 1u && state.ends == 1u);
        CHECK(state.sales_q2_rects == 1u);
        CHECK(fabsf(state.sales_q2_color.red - 0.28f) < 0.001f);
        CHECK(render.uncovered_is_transparent == 1u);

        presentation.flags |= FW_CHART_PRESENTATION_DIRECT_LABELS;
        presentation.label_policy = FW_CHART_LABEL_ALL;
        presentation.max_visible_labels = 32u;
        memset(&state, 0, sizeof(state));
        sink = make_sink(&state);
        services.draw = &sink;
        render.struct_size = sizeof(render);
        CHECK(presentation_api->render(plugin, &fixture.request,
            &presentation, NULL, 0u, bounds, &services, NULL,
            &render) == FW_STATUS_OK);
        CHECK(state.direct_sales_count == 1u);
        CHECK(fabsf(state.direct_sales_anchor.x - 0.90f) < 0.001f);

        presentation.auto_layout = 0u;
        presentation.max_visible_labels = 1u;
        memset(&plan, 0, sizeof(plan));
        plan.struct_size = sizeof(plan);
        CHECK(presentation_api->resolve(plugin, &fixture.request,
            &presentation, &plan) == FW_STATUS_OK);
        CHECK(plan.category_label_stride == 1u &&
            plan.value_label_stride == 1u);
        presentation.label_scale = 0.0f;
        presentation_validation.struct_size = sizeof(presentation_validation);
        CHECK(presentation_api->validate(plugin, &fixture.request,
            &presentation, &presentation_validation) ==
            FW_STATUS_INVALID_ARGUMENT);
    }

    fixture.request.transform.content_rotation_quarter_turns =
        FW_VISUAL_ROTATION_90;
    memset(&state, 0, sizeof(state));
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    CHECK(render.transform.content_rotation_quarter_turns ==
        FW_VISUAL_ROTATION_90);
    CHECK(render.transform.uncovered_is_transparent == 1u);

    fixture = make_fixture(FW_CHART_BAR);
    rebind_fixture(&fixture);
    fixture.request.opacity = 0.0f;
    memset(&state, 0, sizeof(state));
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    CHECK(state.calls == 0u && render.emitted_command_count == 0u);

    fixture = make_fixture(FW_CHART_BAR);
    rebind_fixture(&fixture);
    fixture.request.budget.max_commands = 3u;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &fixture.request, &validation) ==
        FW_STATUS_RESOURCE_LIMIT);
    fixture.request.budget.max_commands = 0u;
    fixture.values_a[0].value = NAN;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &fixture.request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);

    fixture = make_fixture(FW_CHART_BAR);
    rebind_fixture(&fixture);
    fixture.request.target.device_pixel_ratio = 0.0f;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &fixture.request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);
    CHECK(validation.diagnostic_key.length ==
        strlen("chart.invalid_target"));

    fixture = make_fixture(FW_CHART_BAR);
    rebind_fixture(&fixture);
    memset(&state, 0, sizeof(state));
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_OK);
    {
        const uint64_t cache_high = render.cache_key_high;
        const uint64_t cache_low = render.cache_key_low;
        memset(&state, 0, sizeof(state));
        sink = make_sink(&state);
        services.draw = &sink;
        render.struct_size = sizeof(render);
        CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
            &render) == FW_STATUS_OK);
        CHECK(render.cache_key_high == cache_high);
        CHECK(render.cache_key_low == cache_low);
    }

    fixture = make_fixture(FW_CHART_BAR);
    rebind_fixture(&fixture);
    memset(&state, 0, sizeof(state));
    state.fail_at = 4u;
    sink = make_sink(&state);
    services.draw = &sink;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &fixture.request, bounds, &services,
        &render) == FW_STATUS_SINK_REJECTED);
    CHECK(state.begins == 1u && state.ends == 1u);

    plugin_api->unload(plugin);
    puts("core chart renderer contract passed");
    return 0;
}
