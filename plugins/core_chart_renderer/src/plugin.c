/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/chart_element_layer.h>
#include <facetwire/chart_presentation.h>
#include <facetwire/chart_renderer.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CH_MAGIC 0x43485231u
#define CH_MAX_STRING_BYTES 8192u
#define CH_DEFAULT_MAX_CATEGORIES 4096u
#define CH_DEFAULT_MAX_SERIES 64u
#define CH_DEFAULT_MAX_POINTS 262144u
#define CH_DEFAULT_MAX_COMMANDS 1048576u
#define CH_MAX_ELEMENT_OVERRIDES 256u
#define CH_LAYER_MAX_POLYGON_POINTS 16u
#define CH_DENSITY_COLUMNS 12u
#define CH_DENSITY_ROWS 8u
#define CH_MAX_DENSITY_POINTS 4096u
#define CH_MAX_WORDS 256u
#define CH_STYLE_INTERNAL_LEGEND_TAG (UINT32_C(1) << 31)
#define CH_LEGEND_MARKER_TAG "__facetwire.legend.marker"
#define CH_DEFAULT_WIDTH 640.0f
#define CH_DEFAULT_HEIGHT 360.0f
#define CH_PI 3.14159265358979323846
#define CH_MAX_ABS_VALUE 1.0e100
#define CH_FIELD_END(type, field) \
    (offsetof(type, field) + sizeof(((type *)0)->field))

typedef struct ch_context {
    uint32_t magic;
    fw_host_api_v1 host;
} ch_context;

typedef struct ch_limits {
    uint32_t categories;
    uint32_t series;
    uint32_t points;
    uint32_t commands;
} ch_limits;

typedef struct ch_plot {
    float x;
    float y;
    float width;
    float height;
    double minimum;
    double maximum;
    uint32_t visible_series;
} ch_plot;

typedef struct ch_emitter {
    const fw_chart_draw_sink_v1 *sink;
    uint32_t commands;
    fw_status first;
    uint32_t began;
} ch_emitter;

static const fw_capability_descriptor_v1 ch_capabilities[] = {{
    sizeof(fw_capability_descriptor_v1),
    FW_STRING_VIEW_LITERAL(FW_CHART_RENDERER_CAPABILITY_ID),
    FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
    FW_RENDERER_FLAG_DETERMINISTIC | FW_RENDERER_FLAG_HEADLESS |
        FW_RENDERER_FLAG_SEMANTICS | FW_RENDERER_FLAG_HIT_TEST}};

static const fw_plugin_descriptor_v1 ch_descriptor = {
    sizeof(fw_plugin_descriptor_v1), FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.core-chart-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Core Chart Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.3.0"), ch_capabilities, 1u};

static const char ch_parameter_schema[] =
    "{\"schemaVersion\":1,\"parameters\":["
    "{\"id\":\"opacity\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0,\"maximum\":1,\"step\":0.01},"
    "{\"id\":\"kind\",\"type\":\"enum\",\"default\":\"bar\","
    "\"values\":[\"bar\",\"line\",\"pie\",\"area\",\"scatter\","
    "\"bubble\",\"donut\",\"radar\",\"heatmap\",\"gauge\","
    "\"box-plot\",\"histogram\",\"waterfall\",\"funnel\","
    "\"candlestick\",\"time-series\",\"combo\","
    "\"diverging-bar\",\"facet-line\",\"range-area\","
    "\"density-heatmap\",\"word-cloud\",\"rose\"]},"
    "{\"id\":\"orientation\",\"type\":\"enum\","
    "\"default\":\"vertical\",\"values\":[\"vertical\",\"horizontal\"]},"
    "{\"id\":\"stackMode\",\"type\":\"enum\","
    "\"default\":\"none\",\"values\":[\"none\",\"normal\",\"percent\"]},"
    "{\"id\":\"showValueLabels\",\"type\":\"boolean\",\"default\":false},"
    "{\"id\":\"valueLabelMode\",\"type\":\"enum\","
    "\"default\":\"value\",\"values\":[\"value\",\"percent\","
    "\"value-and-percent\"]},"
    "{\"id\":\"fit\",\"type\":\"enum\",\"default\":\"contain\","
    "\"values\":[\"none\",\"contain\",\"cover\",\"fill\"]},"
    "{\"id\":\"contentRotationQuarterTurns\",\"type\":\"integer\","
    "\"default\":0,\"minimum\":0,\"maximum\":3}]}";

static int ch_context_valid(fw_plugin_handle plugin) {
    const ch_context *context = (const ch_context *)plugin;
    return context != NULL && context->magic == CH_MAGIC;
}

static fw_string_view ch_view(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static int ch_string_shape(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int ch_string_equal(fw_string_view value, const char *literal) {
    const size_t length = strlen(literal);
    return value.length == length &&
        (length == 0u || memcmp(value.data, literal, length) == 0);
}

static int ch_view_equal(fw_string_view a, fw_string_view b) {
    return a.length == b.length &&
        (a.length == 0u || memcmp(a.data, b.data, a.length) == 0);
}

static int ch_utf8_valid(fw_string_view value) {
    size_t i = 0u;
    while (i < value.length) {
        const unsigned char first = (unsigned char)value.data[i++];
        size_t count;
        uint32_t cp;
        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            count = 1u; cp = first & 0x1fu;
        } else if (first >= 0xe0u && first <= 0xefu) {
            count = 2u; cp = first & 0x0fu;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            count = 3u; cp = first & 0x07u;
        } else return 0;
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

static int ch_valid_string(fw_string_view value, int required) {
    return ch_string_shape(value) && (!required || value.length != 0u) &&
        value.length <= CH_MAX_STRING_BYTES && ch_utf8_valid(value);
}

static int ch_bool(uint32_t value) { return value <= 1u; }

static int ch_polar_kind(fw_chart_kind kind) {
    return kind == FW_CHART_PIE || kind == FW_CHART_DONUT ||
        kind == FW_CHART_RADAR || kind == FW_CHART_GAUGE ||
        kind == FW_CHART_ROSE;
}

static int ch_single_series_kind(fw_chart_kind kind) {
    return kind == FW_CHART_PIE || kind == FW_CHART_DONUT ||
        kind == FW_CHART_GAUGE || kind == FW_CHART_FUNNEL ||
        kind == FW_CHART_WATERFALL || kind == FW_CHART_CANDLESTICK ||
        kind == FW_CHART_BOX_PLOT || kind == FW_CHART_HISTOGRAM ||
        kind == FW_CHART_WORD_CLOUD || kind == FW_CHART_ROSE;
}

static int ch_cartesian_guides(fw_chart_kind kind) {
    return kind == FW_CHART_BAR || kind == FW_CHART_LINE ||
        kind == FW_CHART_AREA || kind == FW_CHART_HISTOGRAM ||
        kind == FW_CHART_WATERFALL || kind == FW_CHART_BOX_PLOT ||
        kind == FW_CHART_CANDLESTICK ||
        kind == FW_CHART_TIME_SERIES || kind == FW_CHART_COMBO ||
        kind == FW_CHART_HEATMAP || kind == FW_CHART_DIVERGING_BAR ||
        kind == FW_CHART_FACET_LINE || kind == FW_CHART_RANGE_AREA ||
        kind == FW_CHART_DENSITY_HEATMAP;
}

static int ch_valid_color(fw_color_rgba_f32 value) {
    return isfinite(value.red) && value.red >= 0.0f && value.red <= 1.0f &&
        isfinite(value.green) && value.green >= 0.0f && value.green <= 1.0f &&
        isfinite(value.blue) && value.blue >= 0.0f && value.blue <= 1.0f &&
        isfinite(value.alpha) && value.alpha >= 0.0f && value.alpha <= 1.0f;
}

static int ch_valid_size(fw_size_f32 value) {
    return isfinite(value.width) && value.width >= 0.0f &&
        isfinite(value.height) && value.height >= 0.0f;
}

static int ch_valid_rect(fw_rect_f32 value) {
    return isfinite(value.x) && isfinite(value.y) &&
        isfinite(value.width) && value.width >= 0.0f &&
        isfinite(value.height) && value.height >= 0.0f;
}

static int ch_valid_constraints(const fw_layout_constraints_v1 *value) {
    return value->struct_size >= sizeof(*value) &&
        isfinite(value->min_width) && value->min_width >= 0.0f &&
        isfinite(value->max_width) && value->max_width >= value->min_width &&
        isfinite(value->min_height) && value->min_height >= 0.0f &&
        isfinite(value->max_height) && value->max_height >= value->min_height;
}

static uint32_t ch_limit(uint32_t value, uint32_t fallback) {
    return value == 0u ? fallback : value;
}

static ch_limits ch_resolve_limits(const fw_chart_budget_v1 *value) {
    ch_limits result;
    result.categories = ch_limit(value->max_categories,
        CH_DEFAULT_MAX_CATEGORIES);
    result.series = ch_limit(value->max_series, CH_DEFAULT_MAX_SERIES);
    result.points = ch_limit(value->max_points, CH_DEFAULT_MAX_POINTS);
    result.commands = ch_limit(value->max_commands, CH_DEFAULT_MAX_COMMANDS);
    return result;
}

static uint32_t ch_visible_series(const fw_chart_renderer_request_v1 *request) {
    size_t i;
    uint32_t result = 0u;
    for (i = 0u; i < request->series_count; ++i)
        if (request->series[i].visible != 0u) ++result;
    return result;
}

static uint32_t ch_value_count(const fw_chart_renderer_request_v1 *request) {
    size_t i;
    size_t j;
    uint32_t result = 0u;
    for (i = 0u; i < request->series_count; ++i) {
        if (request->series[i].visible == 0u) continue;
        for (j = 0u; j < request->series[i].value_count; ++j)
            if (request->series[i].values[j].missing == 0u) ++result;
    }
    return result;
}

static uint64_t ch_estimated_commands(
    const fw_chart_renderer_request_v1 *request) {
    uint64_t commands = 2u;
    uint64_t values = ch_value_count(request);
    uint64_t visible = ch_visible_series(request);
    if (request->style.show_axes != 0u && !ch_polar_kind(request->kind))
        commands += 2u;
    if (request->style.show_grid != 0u && !ch_polar_kind(request->kind))
        commands += 5u;
    commands += values * 8u + request->category_count * 4u;
    if (request->style.show_labels != 0u)
        commands += 1u + request->category_count;
    if (request->style.show_value_labels != 0u) commands += values;
    if (request->style.show_legend != 0u) commands += visible * 2u;
    return commands;
}

static fw_status ch_validate_request(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request, const char **out_key) {
    size_t i;
    size_t j;
    uint64_t points = 0u;
    uint32_t visible;
    ch_limits limits;
    double pie_total = 0.0;
    *out_key = "chart.invalid_argument";
    if (!ch_context_valid(plugin) || request == NULL ||
        request->struct_size < sizeof(*request) ||
        request->transform.struct_size < sizeof(request->transform) ||
        request->style.struct_size < sizeof(request->style) ||
        request->budget.struct_size < sizeof(request->budget) ||
        request->target.struct_size < sizeof(request->target) ||
        !ch_valid_constraints(&request->constraints)) {
        *out_key = "chart.invalid_struct_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!ch_valid_string(request->zone_id, 0) ||
        !ch_valid_string(request->chart_id, 1) ||
        !ch_valid_string(request->title, 0) ||
        !ch_valid_string(request->summary, 0)) {
        *out_key = "chart.invalid_string";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->kind < FW_CHART_BAR || request->kind > FW_CHART_ROSE ||
        request->categories == NULL || request->category_count == 0u ||
        request->series == NULL || request->series_count == 0u ||
        (ch_single_series_kind(request->kind) &&
            request->series_count != 1u)) {
        *out_key = "chart.invalid_model";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->opacity) || request->opacity < 0.0f ||
        request->opacity > 1.0f || !ch_valid_size(request->intrinsic_size) ||
        (request->intrinsic_size.width == 0.0f) !=
            (request->intrinsic_size.height == 0.0f) ||
        fw_visual_transform_validate(&request->transform) != FW_STATUS_OK ||
        !ch_bool(request->style.show_axes) ||
        !ch_bool(request->style.show_grid) ||
        !ch_bool(request->style.show_legend) ||
        !ch_bool(request->style.show_labels) ||
        !ch_bool(request->style.show_value_labels) ||
        request->style.value_label_mode >
            FW_CHART_VALUE_LABEL_VALUE_AND_PERCENT ||
        request->style.value_precision > 6u ||
        request->style.orientation > FW_CHART_ORIENTATION_HORIZONTAL ||
        request->style.stack_mode > FW_CHART_STACK_PERCENT ||
        !isfinite(request->style.fill_opacity) ||
        request->style.fill_opacity < 0.0f ||
        request->style.fill_opacity > 1.0f ||
        !isfinite(request->style.donut_inner_radius) ||
        request->style.donut_inner_radius < 0.0f ||
        request->style.donut_inner_radius >= 1.0f ||
        !isfinite(request->style.bar_gap_ratio) ||
        request->style.bar_gap_ratio < 0.0f ||
        request->style.bar_gap_ratio >= 1.0f ||
        !isfinite(request->style.line_width) ||
        request->style.line_width <= 0.0f ||
        !isfinite(request->style.point_radius) ||
        request->style.point_radius <= 0.0f ||
        !ch_valid_color(request->style.foreground) ||
        !ch_valid_color(request->style.grid_color)) {
        *out_key = "chart.invalid_geometry";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->target.device_pixel_ratio) ||
        request->target.device_pixel_ratio <= 0.0f ||
        !isfinite(request->target.font_scale) ||
        request->target.font_scale <= 0.0f ||
        request->target.medium < FW_RENDER_MEDIUM_SCREEN ||
        request->target.medium > FW_RENDER_MEDIUM_HEADLESS ||
        !ch_bool(request->target.prefers_dark) ||
        !ch_bool(request->target.high_contrast) ||
        !ch_bool(request->target.reduce_motion) ||
        !ch_bool(request->target.supports_alpha)) {
        *out_key = "chart.invalid_target";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    limits = ch_resolve_limits(&request->budget);
    if (request->category_count > limits.categories ||
        request->series_count > limits.series) {
        *out_key = "chart.resource_limit";
        return FW_STATUS_RESOURCE_LIMIT;
    }
    for (i = 0u; i < request->category_count; ++i) {
        const fw_chart_category_v1 *category = &request->categories[i];
        if (category->struct_size < sizeof(*category) ||
            !ch_valid_string(category->id, 1) ||
            !ch_valid_string(category->label, 0)) {
            *out_key = "chart.invalid_category";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        for (j = 0u; j < i; ++j) {
            if (ch_view_equal(category->id, request->categories[j].id)) {
                *out_key = "chart.duplicate_category";
                return FW_STATUS_INVALID_ARGUMENT;
            }
        }
    }
    visible = 0u;
    for (i = 0u; i < request->series_count; ++i) {
        const fw_chart_series_v1 *series = &request->series[i];
        if (series->struct_size < sizeof(*series) ||
            !ch_valid_string(series->id, 1) ||
            !ch_valid_string(series->label, 0) ||
            series->values == NULL ||
            series->value_count != request->category_count ||
            !ch_valid_color(series->color) || !ch_bool(series->visible) ||
            series->mark > FW_CHART_MARK_SCATTER) {
            *out_key = "chart.invalid_series";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        for (j = 0u; j < i; ++j) {
            if (ch_view_equal(series->id, request->series[j].id)) {
                *out_key = "chart.duplicate_series";
                return FW_STATUS_INVALID_ARGUMENT;
            }
        }
        if (series->visible != 0u) ++visible;
        points += series->value_count;
        if (points > limits.points ||
            (request->kind == FW_CHART_DENSITY_HEATMAP &&
             points > CH_MAX_DENSITY_POINTS)) {
            *out_key = "chart.resource_limit";
            return FW_STATUS_RESOURCE_LIMIT;
        }
        for (j = 0u; j < series->value_count; ++j) {
            const fw_chart_value_v1 *value = &series->values[j];
            if (value->struct_size < sizeof(*value) ||
                !ch_bool(value->missing) ||
                (value->missing == 0u &&
                    (!isfinite(value->value) ||
                     fabs(value->value) > CH_MAX_ABS_VALUE)) ||
                ((request->kind == FW_CHART_PIE ||
                  request->kind == FW_CHART_DONUT ||
                  request->kind == FW_CHART_GAUGE ||
                  request->kind == FW_CHART_FUNNEL) &&
                    value->missing == 0u &&
                    value->value < 0.0)) {
                *out_key = "chart.invalid_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if ((request->kind == FW_CHART_PIE ||
                 request->kind == FW_CHART_DONUT) &&
                value->missing == 0u)
                pie_total += value->value;
            if (value->missing == 0u &&
                (request->kind == FW_CHART_SCATTER ||
                 request->kind == FW_CHART_BUBBLE ||
                 request->kind == FW_CHART_TIME_SERIES) &&
                (!isfinite(value->x) ||
                 (request->kind == FW_CHART_BUBBLE &&
                  (!isfinite(value->size) || value->size < 0.0)))) {
                *out_key = "chart.invalid_xy_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if (value->missing == 0u &&
                request->kind == FW_CHART_BOX_PLOT &&
                (!isfinite(value->minimum) ||
                 !isfinite(value->quartile1) ||
                 !isfinite(value->median) ||
                 !isfinite(value->quartile3) ||
                 !isfinite(value->maximum) ||
                 value->minimum > value->quartile1 ||
                 value->quartile1 > value->median ||
                 value->median > value->quartile3 ||
                 value->quartile3 > value->maximum)) {
                *out_key = "chart.invalid_box_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if (value->missing == 0u &&
                request->kind == FW_CHART_CANDLESTICK &&
                (!isfinite(value->open) || !isfinite(value->high) ||
                 !isfinite(value->low) || !isfinite(value->close) ||
                 value->low > value->open || value->low > value->close ||
                 value->high < value->open || value->high < value->close)) {
                *out_key = "chart.invalid_candlestick_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if (value->missing == 0u &&
                request->kind == FW_CHART_RANGE_AREA &&
                (!isfinite(value->minimum) ||
                 !isfinite(value->maximum) ||
                 value->minimum > value->maximum)) {
                *out_key = "chart.invalid_range_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if (value->missing == 0u &&
                request->kind == FW_CHART_DENSITY_HEATMAP &&
                (!isfinite(value->x) || !isfinite(value->value) ||
                 !isfinite(value->size) || value->size < 0.0)) {
                *out_key = "chart.invalid_density_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
            if (value->missing == 0u &&
                (request->kind == FW_CHART_WORD_CLOUD ||
                 request->kind == FW_CHART_ROSE) &&
                value->value < 0.0) {
                *out_key = "chart.invalid_nonnegative_value";
                return FW_STATUS_INVALID_ARGUMENT;
            }
        }
    }
    if (visible == 0u || ((request->kind == FW_CHART_PIE ||
        request->kind == FW_CHART_DONUT) &&
        (!(pie_total > 0.0) || !isfinite(pie_total)))) {
        *out_key = "chart.no_visible_data";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (ch_estimated_commands(request) > limits.commands) {
        *out_key = "chart.resource_limit";
        return FW_STATUS_RESOURCE_LIMIT;
    }
    *out_key = "chart.valid";
    return FW_STATUS_OK;
}

static fw_size_f32 ch_intrinsic(const fw_chart_renderer_request_v1 *request) {
    if (request->intrinsic_size.width > 0.0f)
        return request->intrinsic_size;
    return (fw_size_f32){CH_DEFAULT_WIDTH, CH_DEFAULT_HEIGHT};
}

static fw_size_f32 ch_measure_size(
    const fw_chart_renderer_request_v1 *request) {
    fw_size_f32 result = ch_intrinsic(request);
    if (result.width < request->constraints.min_width)
        result.width = request->constraints.min_width;
    if (result.width > request->constraints.max_width)
        result.width = request->constraints.max_width;
    if (result.height < request->constraints.min_height)
        result.height = request->constraints.min_height;
    if (result.height > request->constraints.max_height)
        result.height = request->constraints.max_height;
    return result;
}

static ch_plot ch_make_plot(const fw_chart_renderer_request_v1 *request) {
    size_t i;
    size_t j;
    int found = 0;
    ch_plot plot;
    plot.x = 0.12f;
    plot.y = 0.16f;
    plot.width = 0.76f;
    plot.height = 0.62f;
    plot.minimum = 0.0;
    plot.maximum = 0.0;
    plot.visible_series = ch_visible_series(request);
    for (i = 0u; i < request->series_count; ++i) {
        if (request->series[i].visible == 0u) continue;
        for (j = 0u; j < request->series[i].value_count; ++j) {
            const fw_chart_value_v1 *value = &request->series[i].values[j];
            if (value->missing != 0u) continue;
            if (!found) {
                plot.minimum = value->value;
                plot.maximum = value->value;
                found = 1;
            } else {
                if (value->value < plot.minimum) plot.minimum = value->value;
                if (value->value > plot.maximum) plot.maximum = value->value;
            }
        }
    }
    if (request->kind == FW_CHART_BOX_PLOT ||
        request->kind == FW_CHART_CANDLESTICK ||
        request->kind == FW_CHART_RANGE_AREA) {
        found = 0;
        for (j = 0u; j < request->category_count; ++j) {
            const fw_chart_value_v1 *value = &request->series[0].values[j];
            const double low = request->kind == FW_CHART_CANDLESTICK ?
                value->low : value->minimum;
            const double high = request->kind == FW_CHART_CANDLESTICK ?
                value->high : value->maximum;
            if (value->missing != 0u) continue;
            if (!found) {
                plot.minimum = low; plot.maximum = high; found = 1;
            } else {
                if (low < plot.minimum) plot.minimum = low;
                if (high > plot.maximum) plot.maximum = high;
            }
        }
    }
    if ((request->kind == FW_CHART_BAR ||
         request->kind == FW_CHART_AREA) &&
        request->style.stack_mode != FW_CHART_STACK_NONE) {
        plot.minimum = 0.0;
        plot.maximum = request->style.stack_mode ==
            FW_CHART_STACK_PERCENT ? 100.0 : 0.0;
        for (j = 0u; j < request->category_count &&
            request->style.stack_mode == FW_CHART_STACK_NORMAL; ++j) {
            double positive = 0.0;
            double negative = 0.0;
            for (i = 0u; i < request->series_count; ++i) {
                const fw_chart_value_v1 *value =
                    &request->series[i].values[j];
                if (request->series[i].visible == 0u ||
                    value->missing != 0u) continue;
                if (value->value >= 0.0) positive += value->value;
                else negative += value->value;
            }
            if (positive > plot.maximum) plot.maximum = positive;
            if (negative < plot.minimum) plot.minimum = negative;
        }
    }
    if (request->kind == FW_CHART_WATERFALL) {
        double cumulative = 0.0;
        plot.minimum = 0.0;
        plot.maximum = 0.0;
        for (j = 0u; j < request->category_count; ++j) {
            const fw_chart_value_v1 *value = &request->series[0].values[j];
            if (value->missing != 0u) continue;
            cumulative += value->value;
            if (cumulative < plot.minimum) plot.minimum = cumulative;
            if (cumulative > plot.maximum) plot.maximum = cumulative;
        }
    }
    if (plot.minimum > 0.0) plot.minimum = 0.0;
    if (plot.maximum < 0.0) plot.maximum = 0.0;
    if (plot.maximum == plot.minimum) {
        plot.minimum -= 1.0;
        plot.maximum += 1.0;
    }
    return plot;
}

static float ch_value_y(const ch_plot *plot, double value) {
    const double ratio = (value - plot->minimum) /
        (plot->maximum - plot->minimum);
    return plot->y + plot->height - (float)ratio * plot->height;
}

static float ch_value_x(const ch_plot *plot, double value) {
    const double ratio = (value - plot->minimum) /
        (plot->maximum - plot->minimum);
    return plot->x + (float)ratio * plot->width;
}

static fw_color_rgba_f32 ch_with_alpha(fw_color_rgba_f32 color,
    float alpha) {
    color.alpha *= alpha;
    return color;
}

static fw_string_view ch_value_text(double value, uint32_t precision,
    char *buffer, size_t capacity) {
    int length;
    if (precision > 6u) precision = 6u;
    length = snprintf(buffer, capacity, "%.*f", (int)precision, value);
    if (length < 0) return (fw_string_view){NULL, 0u};
    if ((size_t)length >= capacity) length = (int)capacity - 1;
    return (fw_string_view){buffer, (size_t)length};
}

static void ch_record(ch_emitter *emitter, fw_status status);
static int ch_can_emit(const ch_emitter *emitter);

static void ch_emit_value_label(const fw_chart_renderer_request_v1 *request,
    ch_emitter *emitter, double value, fw_point_f32 anchor,
    fw_color_rgba_f32 color, fw_string_view element_id) {
    char buffer[64];
    fw_string_view text;
    if (request->style.show_value_labels == 0u || !ch_can_emit(emitter))
        return;
    text = ch_value_text(value, request->style.value_precision,
        buffer, sizeof(buffer));
    ch_record(emitter, emitter->sink->draw_label(emitter->sink->user_data,
        text, anchor, 0.024f, color, element_id));
}

static void ch_record(ch_emitter *emitter, fw_status status) {
    if (status == FW_STATUS_OK) ++emitter->commands;
    else if (emitter->first == FW_STATUS_OK) emitter->first = status;
}

static int ch_can_emit(const ch_emitter *emitter) {
    return emitter->first == FW_STATUS_OK;
}

static void ch_emit_common(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    uint32_t i;
    size_t category;
    size_t series;
    fw_string_view empty = {NULL, 0u};
    if (ch_cartesian_guides(request->kind) &&
        request->style.show_grid != 0u) {
        for (i = 0u; i < 5u && ch_can_emit(emitter); ++i) {
            const float y = plot->y + plot->height * (float)i / 4.0f;
            ch_record(emitter, emitter->sink->stroke_line(
                emitter->sink->user_data,
                (fw_point_f32){plot->x, y},
                (fw_point_f32){plot->x + plot->width, y}, 0.0015f,
                request->style.grid_color, empty, empty));
        }
    }
    if (ch_cartesian_guides(request->kind) &&
        request->style.show_axes != 0u &&
        ch_can_emit(emitter)) {
        ch_record(emitter, emitter->sink->stroke_line(
            emitter->sink->user_data,
            (fw_point_f32){plot->x, plot->y},
            (fw_point_f32){plot->x, plot->y + plot->height}, 0.002f,
            request->style.foreground, empty, empty));
        if (ch_can_emit(emitter)) ch_record(emitter,
            emitter->sink->stroke_line(emitter->sink->user_data,
                (fw_point_f32){plot->x, plot->y + plot->height},
                (fw_point_f32){plot->x + plot->width,
                    plot->y + plot->height}, 0.002f,
                request->style.foreground, empty, empty));
    }
    if (request->style.show_labels != 0u && ch_can_emit(emitter)) {
        if (request->title.length != 0u) ch_record(emitter,
            emitter->sink->draw_label(emitter->sink->user_data,
                request->title, (fw_point_f32){0.5f, 0.05f}, 0.045f,
                request->style.foreground, request->chart_id));
        if (ch_cartesian_guides(request->kind)) {
            for (category = 0u; category < request->category_count &&
                ch_can_emit(emitter); ++category) {
                const fw_chart_category_v1 *item =
                    &request->categories[category];
                const float x = plot->x + plot->width *
                    ((float)category + 0.5f) /
                    (float)request->category_count;
                ch_record(emitter, emitter->sink->draw_label(
                    emitter->sink->user_data,
                    item->label.length == 0u ? item->id : item->label,
                    (fw_point_f32){x, 0.88f}, 0.028f,
                    request->style.foreground, item->id));
            }
        }
    }
    if (request->style.show_legend != 0u && ch_can_emit(emitter)) {
        uint32_t visible_index = 0u;
        const fw_string_view marker_category =
            (request->style.flags & CH_STYLE_INTERNAL_LEGEND_TAG) != 0u ?
                ch_view(CH_LEGEND_MARKER_TAG) : empty;
        for (series = 0u; series < request->series_count &&
            ch_can_emit(emitter); ++series) {
            const fw_chart_series_v1 *item = &request->series[series];
            float x;
            float slot_width;
            float label_x;
            fw_rect_f32 marker;
            if (item->visible == 0u) continue;
            slot_width = 0.76f / (float)plot->visible_series;
            x = 0.12f + (float)visible_index * slot_width;
            marker = (fw_rect_f32){x + 0.006f, 0.942f, 0.018f, 0.018f};
            label_x = x + 0.030f + fminf(0.12f, slot_width * 0.35f);
            ch_record(emitter, emitter->sink->fill_rect(
                emitter->sink->user_data, marker, item->color,
                item->id, marker_category));
            if (!ch_can_emit(emitter)) break;
            ch_record(emitter, emitter->sink->draw_label(
                emitter->sink->user_data,
                item->label.length == 0u ? item->id : item->label,
                (fw_point_f32){label_x, 0.951f}, 0.026f,
                request->style.foreground, item->id));
            ++visible_index;
        }
    }
}

static void ch_emit_bar(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t category;
    size_t series;
    uint32_t visible_index;
    const int stacked = request->style.stack_mode != FW_CHART_STACK_NONE;
    const int horizontal = request->style.orientation ==
        FW_CHART_ORIENTATION_HORIZONTAL;
    const float group_width = (horizontal ? plot->height : plot->width) /
        (float)request->category_count;
    const float inner = group_width * (1.0f - request->style.bar_gap_ratio);
    const float bar_width = inner /
        (float)(stacked ? 1u : plot->visible_series);
    const float zero_y = ch_value_y(plot, 0.0);
    const float zero_x = ch_value_x(plot, 0.0);
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        double positive = 0.0;
        double negative = 0.0;
        double total = 0.0;
        visible_index = 0u;
        if (request->style.stack_mode == FW_CHART_STACK_PERCENT) {
            for (series = 0u; series < request->series_count; ++series) {
                const fw_chart_value_v1 *item =
                    &request->series[series].values[category];
                if (request->series[series].visible != 0u &&
                    item->missing == 0u) total += fabs(item->value);
            }
            if (total == 0.0) total = 1.0;
        }
        for (series = 0u; series < request->series_count &&
            ch_can_emit(emitter); ++series) {
            const fw_chart_series_v1 *series_value = &request->series[series];
            const fw_chart_value_v1 *value;
            fw_rect_f32 rect;
            double shown;
            double start_value;
            double end_value;
            fw_point_f32 label_anchor;
            if (series_value->visible == 0u ||
                (request->kind == FW_CHART_COMBO &&
                 series_value->mark != FW_CHART_MARK_BAR)) continue;
            value = &series_value->values[category];
            if (value->missing != 0u) { ++visible_index; continue; }
            shown = request->style.stack_mode == FW_CHART_STACK_PERCENT ?
                value->value / total * 100.0 : value->value;
            start_value = 0.0;
            if (stacked) {
                if (shown >= 0.0) {
                    start_value = positive; positive += shown;
                    end_value = positive;
                } else {
                    start_value = negative; negative += shown;
                    end_value = negative;
                }
            } else end_value = shown;
            if (horizontal) {
                const float start_x = stacked ?
                    ch_value_x(plot, start_value) : zero_x;
                const float end_x = ch_value_x(plot, end_value);
                rect.x = start_x < end_x ? start_x : end_x;
                rect.y = plot->y + group_width * (float)category +
                    (group_width - inner) * 0.5f +
                    bar_width * (float)(stacked ? 0u : visible_index);
                rect.width = fabsf(end_x - start_x);
                rect.height = bar_width;
                label_anchor = (fw_point_f32){end_x +
                    (shown >= 0.0 ? 0.018f : -0.018f),
                    rect.y + rect.height * 0.5f};
            } else {
                const float start_y = stacked ?
                    ch_value_y(plot, start_value) : zero_y;
                const float end_y = ch_value_y(plot, end_value);
                rect.x = plot->x + group_width * (float)category +
                    (group_width - inner) * 0.5f +
                    bar_width * (float)(stacked ? 0u : visible_index);
                rect.y = start_y < end_y ? start_y : end_y;
                rect.width = bar_width;
                rect.height = fabsf(end_y - start_y);
                label_anchor = (fw_point_f32){rect.x + rect.width * 0.5f,
                    end_y + (shown >= 0.0 ? -0.024f : 0.024f)};
            }
            ch_record(emitter, emitter->sink->fill_rect(
                emitter->sink->user_data, rect, series_value->color,
                series_value->id, request->categories[category].id));
            ch_emit_value_label(request, emitter, value->value,
                label_anchor, request->style.foreground, series_value->id);
            ++visible_index;
        }
    }
}

static void ch_emit_line(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t series;
    size_t category;
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *series_value = &request->series[series];
        fw_point_f32 previous = {0.0f, 0.0f};
        int has_previous = 0;
        if (series_value->visible == 0u ||
            (request->kind == FW_CHART_COMBO &&
             series_value->mark != FW_CHART_MARK_LINE &&
             series_value->mark != FW_CHART_MARK_SCATTER)) continue;
        for (category = 0u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const fw_chart_value_v1 *value = &series_value->values[category];
            fw_point_f32 point;
            if (value->missing != 0u) { has_previous = 0; continue; }
            point.x = plot->x + plot->width *
                ((float)category + 0.5f) /
                (float)request->category_count;
            point.y = ch_value_y(plot, value->value);
            if (has_previous) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    previous, point, request->style.line_width,
                    series_value->color, series_value->id,
                    request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->fill_circle(emitter->sink->user_data,
                    point, request->style.point_radius,
                    series_value->color, series_value->id,
                    request->categories[category].id));
            ch_emit_value_label(request, emitter, value->value,
                (fw_point_f32){point.x, point.y - 0.028f},
                request->style.foreground, series_value->id);
            previous = point;
            has_previous = 1;
        }
    }
}

static void ch_emit_pie(const fw_chart_renderer_request_v1 *request,
    ch_emitter *emitter) {
    size_t category;
    const fw_chart_series_v1 *series = &request->series[0];
    double total = 0.0;
    double start = -CH_PI * 0.5;
    for (category = 0u; category < request->category_count; ++category)
        if (series->values[category].missing == 0u)
            total += series->values[category].value;
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        double sweep;
        double middle;
        fw_color_rgba_f32 color = series->color;
        if (value->missing != 0u || value->value <= 0.0) continue;
        sweep = value->value / total * CH_PI * 2.0;
        color.red = fmodf(color.red + (float)category * 0.173f, 1.0f);
        color.green = fmodf(color.green + (float)category * 0.271f, 1.0f);
        color.blue = fmodf(color.blue + (float)category * 0.113f, 1.0f);
        middle = start + sweep * 0.5;
        ch_record(emitter, emitter->sink->fill_sector(
            emitter->sink->user_data, (fw_point_f32){0.5f, 0.46f},
            0.34f, request->kind == FW_CHART_DONUT ?
                (request->style.donut_inner_radius > 0.0f ?
                    0.34f * request->style.donut_inner_radius : 0.18f) :
                0.0f,
            (float)start, (float)sweep, color, series->id,
            request->categories[category].id));
        if (request->style.show_labels != 0u && ch_can_emit(emitter)) {
            const fw_chart_category_v1 *item =
                &request->categories[category];
            const fw_point_f32 anchor = {
                0.5f + cosf((float)middle) * 0.22f,
                0.46f + sinf((float)middle) * 0.22f};
            ch_record(emitter, emitter->sink->draw_label(
                emitter->sink->user_data,
                item->label.length == 0u ? item->id : item->label,
                anchor, 0.028f, request->style.foreground, item->id));
        }
        if (request->style.show_value_labels != 0u &&
            ch_can_emit(emitter)) {
            char value_buffer[64];
            char combined[96];
            const double percent = value->value / total * 100.0;
            fw_string_view text;
            if (request->style.value_label_mode ==
                FW_CHART_VALUE_LABEL_PERCENT) {
                const int length = snprintf(combined, sizeof(combined),
                    "%.*f%%", (int)request->style.value_precision, percent);
                text = (fw_string_view){combined,
                    length > 0 ? ((size_t)length < sizeof(combined) ?
                        (size_t)length : sizeof(combined) - 1u) : 0u};
            } else if (request->style.value_label_mode ==
                FW_CHART_VALUE_LABEL_VALUE_AND_PERCENT) {
                const int length = snprintf(combined, sizeof(combined),
                    "%.*f · %.*f%%", (int)request->style.value_precision,
                    value->value, (int)request->style.value_precision,
                    percent);
                text = (fw_string_view){combined,
                    length > 0 ? ((size_t)length < sizeof(combined) ?
                        (size_t)length : sizeof(combined) - 1u) : 0u};
            } else {
                text = ch_value_text(value->value,
                    request->style.value_precision, value_buffer,
                    sizeof(value_buffer));
            }
            ch_record(emitter, emitter->sink->draw_label(
                emitter->sink->user_data, text,
                (fw_point_f32){0.5f + cosf((float)middle) * 0.13f,
                    0.46f + sinf((float)middle) * 0.13f},
                0.022f, request->style.foreground,
                request->categories[category].id));
        }
        start += sweep;
    }
}

static double ch_stack_at(const fw_chart_renderer_request_v1 *request,
    size_t before_series, size_t category) {
    size_t series;
    double result = 0.0;
    double total = 0.0;
    for (series = 0u; series < request->series_count; ++series) {
        const fw_chart_value_v1 *value =
            &request->series[series].values[category];
        if (request->series[series].visible == 0u ||
            value->missing != 0u) continue;
        total += fabs(value->value);
        if (series < before_series) result += value->value;
    }
    if (request->style.stack_mode == FW_CHART_STACK_PERCENT)
        return total == 0.0 ? 0.0 : result / total * 100.0;
    return result;
}

static void ch_emit_area(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t series;
    size_t category;
    const float opacity = request->style.fill_opacity > 0.0f ?
        request->style.fill_opacity : 0.28f;
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        if (item->visible == 0u || (request->kind == FW_CHART_COMBO &&
            item->mark != FW_CHART_MARK_AREA)) continue;
        for (category = 1u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const fw_chart_value_v1 *previous = &item->values[category - 1u];
            const fw_chart_value_v1 *current = &item->values[category];
            double lower_previous = 0.0;
            double lower_current = 0.0;
            double upper_previous;
            double upper_current;
            fw_point_f32 polygon[4];
            const float x0 = plot->x + plot->width *
                ((float)category - 0.5f) /
                (float)request->category_count;
            const float x1 = plot->x + plot->width *
                ((float)category + 0.5f) /
                (float)request->category_count;
            if (previous->missing != 0u || current->missing != 0u) continue;
            if (request->style.stack_mode != FW_CHART_STACK_NONE) {
                lower_previous = ch_stack_at(request, series, category - 1u);
                lower_current = ch_stack_at(request, series, category);
            }
            upper_previous = lower_previous + previous->value;
            upper_current = lower_current + current->value;
            if (request->style.stack_mode == FW_CHART_STACK_PERCENT) {
                upper_previous = ch_stack_at(request, series + 1u,
                    category - 1u);
                upper_current = ch_stack_at(request, series + 1u, category);
            }
            polygon[0] = (fw_point_f32){x0,
                ch_value_y(plot, lower_previous)};
            polygon[1] = (fw_point_f32){x0,
                ch_value_y(plot, upper_previous)};
            polygon[2] = (fw_point_f32){x1,
                ch_value_y(plot, upper_current)};
            polygon[3] = (fw_point_f32){x1,
                ch_value_y(plot, lower_current)};
            ch_record(emitter, emitter->sink->fill_polygon(
                emitter->sink->user_data, polygon, 4u,
                ch_with_alpha(item->color, opacity), item->id,
                request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    polygon[1], polygon[2], request->style.line_width,
                    item->color, item->id,
                    request->categories[category].id));
            ch_emit_value_label(request, emitter, current->value,
                (fw_point_f32){x1, polygon[2].y - 0.025f},
                request->style.foreground, item->id);
        }
    }
}

static void ch_xy_range(const fw_chart_renderer_request_v1 *request,
    double *minimum, double *maximum, double *largest_size) {
    size_t series;
    size_t category;
    int found = 0;
    *minimum = 0.0; *maximum = 1.0; *largest_size = 1.0;
    for (series = 0u; series < request->series_count; ++series) {
        if (request->series[series].visible == 0u) continue;
        for (category = 0u; category < request->category_count; ++category) {
            const fw_chart_value_v1 *value =
                &request->series[series].values[category];
            if (value->missing != 0u) continue;
            if (!found) { *minimum = value->x; *maximum = value->x; found = 1; }
            else {
                if (value->x < *minimum) *minimum = value->x;
                if (value->x > *maximum) *maximum = value->x;
            }
            if (value->size > *largest_size) *largest_size = value->size;
        }
    }
    if (*minimum == *maximum) *maximum = *minimum + 1.0;
}

static void ch_emit_scatter(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter, int connect) {
    size_t series;
    size_t category;
    double minimum;
    double maximum;
    double largest;
    ch_xy_range(request, &minimum, &maximum, &largest);
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        fw_point_f32 previous = {0.0f, 0.0f};
        int has_previous = 0;
        if (item->visible == 0u) continue;
        for (category = 0u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const fw_chart_value_v1 *value = &item->values[category];
            fw_point_f32 point;
            float radius = request->style.point_radius;
            if (value->missing != 0u) { has_previous = 0; continue; }
            point.x = plot->x + (float)((value->x - minimum) /
                (maximum - minimum)) * plot->width;
            point.y = ch_value_y(plot, value->value);
            if (request->kind == FW_CHART_BUBBLE)
                radius = 0.012f + 0.035f *
                    (float)sqrt(value->size / largest);
            if (connect && has_previous) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    previous, point, request->style.line_width,
                    item->color, item->id,
                    request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->fill_circle(emitter->sink->user_data,
                    point, radius, item->color, item->id,
                    request->categories[category].id));
            ch_emit_value_label(request, emitter, value->value,
                (fw_point_f32){point.x, point.y - radius - 0.018f},
                request->style.foreground, item->id);
            previous = point;
            has_previous = 1;
        }
    }
}

static void ch_emit_radar(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t category;
    size_t series;
    const fw_point_f32 center = {0.5f, 0.48f};
    const float radius = 0.31f;
    const double range = plot->maximum - plot->minimum;
    const fw_string_view empty = {NULL, 0u};
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const double angle = -CH_PI * 0.5 + CH_PI * 2.0 *
            (double)category / (double)request->category_count;
        const fw_point_f32 edge = {center.x + cosf((float)angle) * radius,
            center.y + sinf((float)angle) * radius};
        ch_record(emitter, emitter->sink->stroke_line(
            emitter->sink->user_data, center, edge, 0.0015f,
            request->style.grid_color, empty, empty));
        if (request->style.show_labels != 0u && ch_can_emit(emitter))
            ch_record(emitter, emitter->sink->draw_label(
                emitter->sink->user_data,
                request->categories[category].label.length != 0u ?
                    request->categories[category].label :
                    request->categories[category].id,
                (fw_point_f32){center.x + cosf((float)angle) * 0.38f,
                    center.y + sinf((float)angle) * 0.38f},
                0.025f, request->style.foreground,
                request->categories[category].id));
    }
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        if (item->visible == 0u) continue;
        for (category = 0u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const size_t next = (category + 1u) % request->category_count;
            const fw_chart_value_v1 *a = &item->values[category];
            const fw_chart_value_v1 *b = &item->values[next];
            fw_point_f32 triangle[3];
            double angle_a;
            double angle_b;
            float ratio_a;
            float ratio_b;
            if (a->missing != 0u || b->missing != 0u) continue;
            angle_a = -CH_PI * 0.5 + CH_PI * 2.0 *
                (double)category / (double)request->category_count;
            angle_b = -CH_PI * 0.5 + CH_PI * 2.0 *
                (double)next / (double)request->category_count;
            ratio_a = (float)((a->value - plot->minimum) / range);
            ratio_b = (float)((b->value - plot->minimum) / range);
            triangle[0] = center;
            triangle[1] = (fw_point_f32){center.x +
                cosf((float)angle_a) * radius * ratio_a,
                center.y + sinf((float)angle_a) * radius * ratio_a};
            triangle[2] = (fw_point_f32){center.x +
                cosf((float)angle_b) * radius * ratio_b,
                center.y + sinf((float)angle_b) * radius * ratio_b};
            ch_record(emitter, emitter->sink->fill_polygon(
                emitter->sink->user_data, triangle, 3u,
                ch_with_alpha(item->color, 0.22f), item->id,
                request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    triangle[1], triangle[2], request->style.line_width,
                    item->color, item->id,
                    request->categories[category].id));
        }
    }
}

static void ch_emit_heatmap(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t series;
    size_t category;
    const float width = plot->width / (float)request->category_count;
    const float height = plot->height / (float)plot->visible_series;
    const double range = plot->maximum - plot->minimum;
    uint32_t visible = 0u;
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        if (item->visible == 0u) continue;
        for (category = 0u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const fw_chart_value_v1 *value = &item->values[category];
            float intensity;
            fw_color_rgba_f32 color;
            fw_rect_f32 rect;
            if (value->missing != 0u) continue;
            intensity = (float)((value->value - plot->minimum) / range);
            color = item->color;
            color.alpha *= 0.18f + intensity * 0.82f;
            rect = (fw_rect_f32){plot->x + width * (float)category,
                plot->y + height * (float)visible, width, height};
            ch_record(emitter, emitter->sink->fill_rect(
                emitter->sink->user_data, rect, color, item->id,
                request->categories[category].id));
            ch_emit_value_label(request, emitter, value->value,
                (fw_point_f32){rect.x + rect.width * 0.5f,
                    rect.y + rect.height * 0.5f},
                request->style.foreground, item->id);
        }
        ++visible;
    }
}

static void ch_emit_gauge(const fw_chart_renderer_request_v1 *request,
    ch_emitter *emitter) {
    const fw_chart_series_v1 *series = &request->series[0];
    const fw_chart_value_v1 *value = &series->values[0];
    const double ratio = value->value < 0.0 ? 0.0 :
        (value->value > 100.0 ? 1.0 : value->value / 100.0);
    const fw_point_f32 center = {0.5f, 0.52f};
    const float start = (float)(CH_PI * 0.75);
    const float sweep = (float)(CH_PI * 1.5);
    fw_color_rgba_f32 track = request->style.grid_color;
    track.alpha *= 0.45f;
    ch_record(emitter, emitter->sink->fill_sector(
        emitter->sink->user_data, center, 0.34f, 0.24f,
        start, sweep, track, series->id, request->categories[0].id));
    if (ch_can_emit(emitter)) ch_record(emitter,
        emitter->sink->fill_sector(emitter->sink->user_data, center,
            0.34f, 0.24f, start, sweep * (float)ratio, series->color,
            series->id, request->categories[0].id));
    ch_emit_value_label(request, emitter, value->value,
        center, request->style.foreground, series->id);
}

static void ch_emit_box(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t category;
    const fw_chart_series_v1 *series = &request->series[0];
    const float group = plot->width / (float)request->category_count;
    const fw_string_view empty = {NULL, 0u};
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        const float center = plot->x + group * ((float)category + 0.5f);
        const float half = group * 0.26f;
        fw_rect_f32 box;
        if (value->missing != 0u) continue;
        box = (fw_rect_f32){center - half, ch_value_y(plot, value->quartile3),
            half * 2.0f, fabsf(ch_value_y(plot, value->quartile1) -
                ch_value_y(plot, value->quartile3))};
        ch_record(emitter, emitter->sink->stroke_line(
            emitter->sink->user_data,
            (fw_point_f32){center, ch_value_y(plot, value->minimum)},
            (fw_point_f32){center, ch_value_y(plot, value->maximum)},
            request->style.line_width, series->color, series->id,
            request->categories[category].id));
        if (ch_can_emit(emitter)) ch_record(emitter,
            emitter->sink->fill_rect(emitter->sink->user_data, box,
                ch_with_alpha(series->color, 0.35f), series->id,
                request->categories[category].id));
        if (ch_can_emit(emitter)) ch_record(emitter,
            emitter->sink->stroke_line(emitter->sink->user_data,
                (fw_point_f32){center - half,
                    ch_value_y(plot, value->median)},
                (fw_point_f32){center + half,
                    ch_value_y(plot, value->median)},
                request->style.line_width * 1.5f, series->color,
                series->id, empty));
        ch_emit_value_label(request, emitter, value->median,
            (fw_point_f32){center, box.y - 0.025f},
            request->style.foreground, series->id);
    }
}

static void ch_emit_waterfall(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t category;
    const fw_chart_series_v1 *series = &request->series[0];
    const float group = plot->width / (float)request->category_count;
    double cumulative = 0.0;
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        double next;
        float start_y;
        float end_y;
        fw_rect_f32 rect;
        fw_color_rgba_f32 color = series->color;
        if (value->missing != 0u) continue;
        next = cumulative + value->value;
        start_y = ch_value_y(plot, cumulative);
        end_y = ch_value_y(plot, next);
        if (value->value < 0.0)
            color = (fw_color_rgba_f32){0.91f, 0.30f, 0.24f, color.alpha};
        rect = (fw_rect_f32){plot->x + group * (float)category + group * 0.16f,
            start_y < end_y ? start_y : end_y, group * 0.68f,
            fabsf(end_y - start_y)};
        ch_record(emitter, emitter->sink->fill_rect(
            emitter->sink->user_data, rect, color, series->id,
            request->categories[category].id));
        if (category + 1u < request->category_count && ch_can_emit(emitter))
            ch_record(emitter, emitter->sink->stroke_line(
                emitter->sink->user_data,
                (fw_point_f32){rect.x + rect.width, end_y},
                (fw_point_f32){rect.x + group, end_y}, 0.0015f,
                request->style.grid_color, series->id,
                request->categories[category].id));
        ch_emit_value_label(request, emitter, value->value,
            (fw_point_f32){rect.x + rect.width * 0.5f, end_y - 0.024f},
            request->style.foreground, series->id);
        cumulative = next;
    }
}

static void ch_emit_funnel(const fw_chart_renderer_request_v1 *request,
    ch_emitter *emitter) {
    size_t category;
    const fw_chart_series_v1 *series = &request->series[0];
    double maximum = 0.0;
    const float top = 0.16f;
    const float height = 0.66f / (float)request->category_count;
    for (category = 0u; category < request->category_count; ++category)
        if (series->values[category].missing == 0u &&
            series->values[category].value > maximum)
            maximum = series->values[category].value;
    if (maximum <= 0.0) return;
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        const fw_chart_value_v1 *next = category + 1u <
            request->category_count ? &series->values[category + 1u] : value;
        const float upper = 0.72f * (float)(value->value / maximum);
        const float lower = 0.72f * (float)(next->value / maximum);
        const float y = top + height * (float)category;
        fw_point_f32 polygon[4];
        fw_color_rgba_f32 color = series->color;
        if (value->missing != 0u) continue;
        color.red = fmodf(color.red + (float)category * 0.09f, 1.0f);
        color.green = fmodf(color.green + (float)category * 0.05f, 1.0f);
        polygon[0] = (fw_point_f32){0.5f - upper * 0.5f, y};
        polygon[1] = (fw_point_f32){0.5f + upper * 0.5f, y};
        polygon[2] = (fw_point_f32){0.5f + lower * 0.5f, y + height};
        polygon[3] = (fw_point_f32){0.5f - lower * 0.5f, y + height};
        ch_record(emitter, emitter->sink->fill_polygon(
            emitter->sink->user_data, polygon, 4u, color, series->id,
            request->categories[category].id));
        ch_emit_value_label(request, emitter, value->value,
            (fw_point_f32){0.5f, y + height * 0.5f},
            request->style.foreground, series->id);
    }
}

static void ch_emit_candlestick(
    const fw_chart_renderer_request_v1 *request, const ch_plot *plot,
    ch_emitter *emitter) {
    size_t category;
    const fw_chart_series_v1 *series = &request->series[0];
    const float group = plot->width / (float)request->category_count;
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        const float center = plot->x + group * ((float)category + 0.5f);
        fw_color_rgba_f32 color = value->close >= value->open ?
            (fw_color_rgba_f32){0.08f, 0.63f, 0.42f, 1.0f} :
            (fw_color_rgba_f32){0.91f, 0.30f, 0.24f, 1.0f};
        fw_rect_f32 body;
        float open_y;
        float close_y;
        if (value->missing != 0u) continue;
        open_y = ch_value_y(plot, value->open);
        close_y = ch_value_y(plot, value->close);
        body = (fw_rect_f32){center - group * 0.22f,
            open_y < close_y ? open_y : close_y, group * 0.44f,
            fmaxf(0.004f, fabsf(open_y - close_y))};
        ch_record(emitter, emitter->sink->stroke_line(
            emitter->sink->user_data,
            (fw_point_f32){center, ch_value_y(plot, value->high)},
            (fw_point_f32){center, ch_value_y(plot, value->low)},
            request->style.line_width, color, series->id,
            request->categories[category].id));
        if (ch_can_emit(emitter)) ch_record(emitter,
            emitter->sink->fill_rect(emitter->sink->user_data, body,
                color, series->id, request->categories[category].id));
        ch_emit_value_label(request, emitter, value->close,
            (fw_point_f32){center, ch_value_y(plot, value->high) - 0.025f},
            request->style.foreground, series->id);
    }
}

static void ch_emit_diverging_bar(
    const fw_chart_renderer_request_v1 *request, const ch_plot *plot,
    ch_emitter *emitter) {
    fw_chart_renderer_request_v1 local = *request;
    local.style = request->style;
    local.style.orientation = FW_CHART_ORIENTATION_HORIZONTAL;
    local.style.stack_mode = FW_CHART_STACK_NONE;
    ch_emit_bar(&local, plot, emitter);
}

static void ch_emit_facet_line(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, ch_emitter *emitter) {
    size_t series;
    size_t category;
    uint32_t visible_index = 0u;
    const float panel_height = plot->height /
        (float)plot->visible_series;
    const fw_string_view empty = {NULL, 0u};
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        double minimum = 0.0;
        double maximum = 0.0;
        int found = 0;
        fw_point_f32 previous = {0.0f, 0.0f};
        int has_previous = 0;
        const float top = plot->y + panel_height * (float)visible_index;
        if (item->visible == 0u) continue;
        for (category = 0u; category < request->category_count; ++category) {
            const fw_chart_value_v1 *value = &item->values[category];
            if (value->missing != 0u) continue;
            if (!found) {
                minimum = maximum = value->value;
                found = 1;
            } else {
                if (value->value < minimum) minimum = value->value;
                if (value->value > maximum) maximum = value->value;
            }
        }
        if (!found) { ++visible_index; continue; }
        if (minimum == maximum) { minimum -= 1.0; maximum += 1.0; }
        if (visible_index != 0u) ch_record(emitter,
            emitter->sink->stroke_line(emitter->sink->user_data,
                (fw_point_f32){plot->x, top},
                (fw_point_f32){plot->x + plot->width, top}, 0.001f,
                request->style.grid_color, empty, empty));
        if (request->style.show_labels != 0u && ch_can_emit(emitter))
            ch_record(emitter, emitter->sink->draw_label(
                emitter->sink->user_data,
                item->label.length != 0u ? item->label : item->id,
                (fw_point_f32){plot->x + 0.01f, top + 0.025f},
                0.023f, item->color, item->id));
        for (category = 0u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const fw_chart_value_v1 *value = &item->values[category];
            fw_point_f32 point;
            if (value->missing != 0u) { has_previous = 0; continue; }
            point.x = plot->x + plot->width *
                ((float)category + 0.5f) /
                (float)request->category_count;
            point.y = top + panel_height * 0.84f -
                (float)((value->value - minimum) /
                    (maximum - minimum)) * panel_height * 0.68f;
            if (has_previous) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    previous, point, request->style.line_width,
                    item->color, item->id,
                    request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->fill_circle(emitter->sink->user_data,
                    point, request->style.point_radius * 0.8f,
                    item->color, item->id,
                    request->categories[category].id));
            previous = point;
            has_previous = 1;
        }
        ++visible_index;
    }
}

static void ch_emit_range_area(
    const fw_chart_renderer_request_v1 *request, const ch_plot *plot,
    ch_emitter *emitter) {
    size_t series;
    size_t category;
    const float opacity = request->style.fill_opacity > 0.0f ?
        request->style.fill_opacity : 0.24f;
    for (series = 0u; series < request->series_count &&
        ch_can_emit(emitter); ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        if (item->visible == 0u) continue;
        for (category = 1u; category < request->category_count &&
            ch_can_emit(emitter); ++category) {
            const fw_chart_value_v1 *a = &item->values[category - 1u];
            const fw_chart_value_v1 *b = &item->values[category];
            const float x0 = plot->x + plot->width *
                ((float)category - 0.5f) /
                (float)request->category_count;
            const float x1 = plot->x + plot->width *
                ((float)category + 0.5f) /
                (float)request->category_count;
            fw_point_f32 polygon[4];
            if (a->missing != 0u || b->missing != 0u) continue;
            polygon[0] = (fw_point_f32){x0,
                ch_value_y(plot, a->minimum)};
            polygon[1] = (fw_point_f32){x0,
                ch_value_y(plot, a->maximum)};
            polygon[2] = (fw_point_f32){x1,
                ch_value_y(plot, b->maximum)};
            polygon[3] = (fw_point_f32){x1,
                ch_value_y(plot, b->minimum)};
            ch_record(emitter, emitter->sink->fill_polygon(
                emitter->sink->user_data, polygon, 4u,
                ch_with_alpha(item->color, opacity), item->id,
                request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    polygon[1], polygon[2], request->style.line_width,
                    item->color, item->id,
                    request->categories[category].id));
            if (ch_can_emit(emitter)) ch_record(emitter,
                emitter->sink->stroke_line(emitter->sink->user_data,
                    polygon[0], polygon[3], request->style.line_width,
                    item->color, item->id,
                    request->categories[category].id));
            ch_emit_value_label(request, emitter,
                (b->minimum + b->maximum) * 0.5,
                (fw_point_f32){x1,
                    ch_value_y(plot, b->maximum) - 0.02f},
                request->style.foreground, item->id);
        }
    }
}

static double ch_density_at(const fw_chart_renderer_request_v1 *request,
    double x, double y, double min_x, double max_x,
    double min_y, double max_y) {
    size_t series;
    size_t category;
    double result = 0.0;
    const double span_x = max_x - min_x;
    const double span_y = max_y - min_y;
    for (series = 0u; series < request->series_count; ++series) {
        const fw_chart_series_v1 *item = &request->series[series];
        if (item->visible == 0u) continue;
        for (category = 0u; category < request->category_count; ++category) {
            const fw_chart_value_v1 *value = &item->values[category];
            double dx;
            double dy;
            double distance;
            if (value->missing != 0u) continue;
            dx = (value->x - x) / span_x;
            dy = (value->value - y) / span_y;
            distance = dx * dx + dy * dy;
            if (distance < 0.09)
                result += (value->size > 0.0 ? value->size : 1.0) *
                    exp(-distance / 0.018);
        }
    }
    return result;
}

static fw_color_rgba_f32 ch_density_color(double ratio) {
    fw_color_rgba_f32 color;
    if (ratio < 0.33) {
        const float t = (float)(ratio / 0.33);
        color = (fw_color_rgba_f32){0.18f, 0.42f + 0.45f * t,
            0.95f, 0.12f + 0.55f * t};
    } else if (ratio < 0.66) {
        const float t = (float)((ratio - 0.33) / 0.33);
        color = (fw_color_rgba_f32){0.2f + 0.75f * t,
            0.87f, 0.75f - 0.55f * t, 0.67f + 0.2f * t};
    } else {
        const float t = (float)((ratio - 0.66) / 0.34);
        color = (fw_color_rgba_f32){0.95f, 0.87f - 0.72f * t,
            0.2f - 0.12f * t, 0.87f};
    }
    return color;
}

static void ch_emit_density_heatmap(
    const fw_chart_renderer_request_v1 *request, const ch_plot *plot,
    ch_emitter *emitter) {
    uint32_t row;
    uint32_t column;
    double min_x;
    double max_x;
    double unused;
    double min_y = 0.0;
    double max_y = 0.0;
    double maximum_density = 0.0;
    int found_y = 0;
    size_t series;
    size_t category;
    const fw_string_view empty = {NULL, 0u};
    ch_xy_range(request, &min_x, &max_x, &unused);
    for (series = 0u; series < request->series_count; ++series)
        for (category = 0u; category < request->category_count; ++category) {
            const fw_chart_value_v1 *value =
                &request->series[series].values[category];
            if (request->series[series].visible == 0u ||
                value->missing != 0u) continue;
            if (!found_y) { min_y = max_y = value->value; found_y = 1; }
            else {
                if (value->value < min_y) min_y = value->value;
                if (value->value > max_y) max_y = value->value;
            }
        }
    if (!found_y) return;
    if (min_y == max_y) max_y = min_y + 1.0;
    for (row = 0u; row < CH_DENSITY_ROWS; ++row)
        for (column = 0u; column < CH_DENSITY_COLUMNS; ++column) {
            const double x = min_x + (max_x - min_x) *
                ((double)column + 0.5) / CH_DENSITY_COLUMNS;
            const double y = min_y + (max_y - min_y) *
                ((double)row + 0.5) / CH_DENSITY_ROWS;
            const double density = ch_density_at(request, x, y,
                min_x, max_x, min_y, max_y);
            if (density > maximum_density) maximum_density = density;
        }
    if (maximum_density <= 0.0) return;
    for (row = 0u; row < CH_DENSITY_ROWS && ch_can_emit(emitter); ++row)
        for (column = 0u; column < CH_DENSITY_COLUMNS &&
            ch_can_emit(emitter); ++column) {
            const double x = min_x + (max_x - min_x) *
                ((double)column + 0.5) / CH_DENSITY_COLUMNS;
            const double y = min_y + (max_y - min_y) *
                ((double)row + 0.5) / CH_DENSITY_ROWS;
            const double density = ch_density_at(request, x, y,
                min_x, max_x, min_y, max_y);
            const fw_rect_f32 rect = {
                plot->x + plot->width * (float)column / CH_DENSITY_COLUMNS,
                plot->y + plot->height *
                    (float)(CH_DENSITY_ROWS - row - 1u) / CH_DENSITY_ROWS,
                plot->width / CH_DENSITY_COLUMNS,
                plot->height / CH_DENSITY_ROWS};
            ch_record(emitter, emitter->sink->fill_rect(
                emitter->sink->user_data, rect,
                ch_density_color(density / maximum_density),
                request->series[0].id, empty));
        }
}

static size_t ch_utf8_codepoints(fw_string_view value) {
    size_t i;
    size_t result = 0u;
    for (i = 0u; i < value.length; ++i)
        if (((unsigned char)value.data[i] & 0xc0u) != 0x80u) ++result;
    return result;
}

typedef struct ch_word_box {
    float left;
    float top;
    float right;
    float bottom;
} ch_word_box;

static int ch_word_intersects(const ch_word_box *a, const ch_word_box *b) {
    return a->left < b->right && a->right > b->left &&
        a->top < b->bottom && a->bottom > b->top;
}

static void ch_emit_word_cloud(
    const fw_chart_renderer_request_v1 *request, ch_emitter *emitter) {
    const fw_chart_series_v1 *series = &request->series[0];
    size_t order[CH_MAX_WORDS];
    ch_word_box boxes[CH_MAX_WORDS];
    size_t count = request->category_count < CH_MAX_WORDS ?
        request->category_count : CH_MAX_WORDS;
    size_t i;
    size_t j;
    size_t placed = 0u;
    double maximum = 0.0;
    for (i = 0u; i < count; ++i) order[i] = i;
    for (i = 1u; i < count; ++i) {
        const size_t selected = order[i];
        j = i;
        while (j > 0u && series->values[order[j - 1u]].value <
            series->values[selected].value) {
            order[j] = order[j - 1u];
            --j;
        }
        order[j] = selected;
    }
    for (i = 0u; i < count; ++i)
        if (series->values[order[i]].missing == 0u &&
            series->values[order[i]].value > maximum)
            maximum = series->values[order[i]].value;
    if (maximum <= 0.0) return;
    for (i = 0u; i < count && ch_can_emit(emitter); ++i) {
        const size_t index = order[i];
        const fw_chart_value_v1 *value = &series->values[index];
        const fw_chart_category_v1 *category = &request->categories[index];
        const fw_string_view text = category->label.length != 0u ?
            category->label : category->id;
        const float font = 0.018f + 0.075f *
            sqrtf((float)(value->value / maximum));
        const float width = fminf(0.46f,
            (float)ch_utf8_codepoints(text) * font * 0.56f);
        const float height = font * 1.18f;
        uint32_t attempt;
        if (value->missing != 0u || value->value <= 0.0) continue;
        for (attempt = 0u; attempt < 320u; ++attempt) {
            const float angle = (float)attempt * 0.57f;
            const float radius = 0.0021f * (float)attempt;
            const fw_point_f32 anchor = {0.5f + cosf(angle) * radius * 1.4f,
                0.48f + sinf(angle) * radius};
            ch_word_box candidate = {anchor.x - width * 0.5f,
                anchor.y - height * 0.5f, anchor.x + width * 0.5f,
                anchor.y + height * 0.5f};
            int collision = candidate.left < 0.04f ||
                candidate.right > 0.96f || candidate.top < 0.10f ||
                candidate.bottom > 0.88f;
            for (j = 0u; j < placed && !collision; ++j)
                collision = ch_word_intersects(&candidate, &boxes[j]);
            if (!collision) {
                fw_color_rgba_f32 color = series->color;
                color.red = fmodf(color.red + (float)i * 0.121f, 1.0f);
                color.green = fmodf(color.green + (float)i * 0.071f, 1.0f);
                boxes[placed++] = candidate;
                ch_record(emitter, emitter->sink->draw_label(
                    emitter->sink->user_data, text, anchor, font, color,
                    category->id));
                break;
            }
        }
    }
}

static void ch_emit_rose(const fw_chart_renderer_request_v1 *request,
    ch_emitter *emitter) {
    const fw_chart_series_v1 *series = &request->series[0];
    const fw_point_f32 center = {0.5f, 0.48f};
    const float sweep = (float)(CH_PI * 2.0 /
        (double)request->category_count);
    double maximum = 0.0;
    size_t category;
    for (category = 0u; category < request->category_count; ++category)
        if (series->values[category].missing == 0u &&
            series->values[category].value > maximum)
            maximum = series->values[category].value;
    if (maximum <= 0.0) return;
    for (category = 0u; category < request->category_count &&
        ch_can_emit(emitter); ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        fw_color_rgba_f32 color = series->color;
        float radius;
        float start;
        if (value->missing != 0u || value->value <= 0.0) continue;
        radius = 0.08f + 0.30f * sqrtf((float)(value->value / maximum));
        start = (float)(-CH_PI * 0.5) + sweep * (float)category;
        color.red = fmodf(color.red + (float)category * 0.137f, 1.0f);
        color.green = fmodf(color.green + (float)category * 0.083f, 1.0f);
        ch_record(emitter, emitter->sink->fill_sector(
            emitter->sink->user_data, center, radius, 0.025f,
            start + 0.012f, sweep - 0.024f, color, series->id,
            request->categories[category].id));
        ch_emit_value_label(request, emitter, value->value,
            (fw_point_f32){center.x + cosf(start + sweep * 0.5f) *
                radius * 0.62f,
                center.y + sinf(start + sweep * 0.5f) * radius * 0.62f},
            request->style.foreground, series->id);
    }
}

static void ch_hash(uint64_t *high, uint64_t *low,
    const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0u; i < length; ++i) {
        *low ^= bytes[i];
        *low *= UINT64_C(1099511628211);
        *high ^= *low + UINT64_C(0x9e3779b97f4a7c15) +
            (*high << 6u) + (*high >> 2u);
    }
}

static void ch_hash_view(uint64_t *high, uint64_t *low,
    fw_string_view value) {
    ch_hash(high, low, &value.length, sizeof(value.length));
    ch_hash(high, low, value.data, value.length);
}

static void ch_cache_key(const fw_chart_renderer_request_v1 *request,
    fw_rect_f32 bounds, uint64_t *high, uint64_t *low) {
    size_t i;
    size_t j;
    *high = UINT64_C(7809847782465536322);
    *low = UINT64_C(1469598103934665603);
    ch_hash_view(high, low, request->chart_id);
    ch_hash(high, low, &request->kind, sizeof(request->kind));
    ch_hash(high, low, &request->opacity, sizeof(request->opacity));
    ch_hash(high, low, &request->transform.fit,
        sizeof(request->transform.fit));
    ch_hash(high, low, &request->transform.alignment_x,
        sizeof(request->transform.alignment_x));
    ch_hash(high, low, &request->transform.alignment_y,
        sizeof(request->transform.alignment_y));
    ch_hash(high, low, &request->transform.clip,
        sizeof(request->transform.clip));
    ch_hash(high, low, &request->transform.content_rotation_quarter_turns,
        sizeof(request->transform.content_rotation_quarter_turns));
    ch_hash(high, low, &request->style.show_axes,
        sizeof(request->style.show_axes));
    ch_hash(high, low, &request->style.show_grid,
        sizeof(request->style.show_grid));
    ch_hash(high, low, &request->style.show_legend,
        sizeof(request->style.show_legend));
    ch_hash(high, low, &request->style.show_labels,
        sizeof(request->style.show_labels));
    ch_hash(high, low, &request->style.bar_gap_ratio,
        sizeof(request->style.bar_gap_ratio));
    ch_hash(high, low, &request->style.line_width,
        sizeof(request->style.line_width));
    ch_hash(high, low, &request->style.point_radius,
        sizeof(request->style.point_radius));
    ch_hash(high, low, &request->style.foreground.red,
        sizeof(request->style.foreground.red));
    ch_hash(high, low, &request->style.foreground.green,
        sizeof(request->style.foreground.green));
    ch_hash(high, low, &request->style.foreground.blue,
        sizeof(request->style.foreground.blue));
    ch_hash(high, low, &request->style.foreground.alpha,
        sizeof(request->style.foreground.alpha));
    ch_hash(high, low, &request->style.grid_color.red,
        sizeof(request->style.grid_color.red));
    ch_hash(high, low, &request->style.grid_color.green,
        sizeof(request->style.grid_color.green));
    ch_hash(high, low, &request->style.grid_color.blue,
        sizeof(request->style.grid_color.blue));
    ch_hash(high, low, &request->style.grid_color.alpha,
        sizeof(request->style.grid_color.alpha));
    ch_hash(high, low, &request->style.show_value_labels,
        sizeof(request->style.show_value_labels));
    ch_hash(high, low, &request->style.value_label_mode,
        sizeof(request->style.value_label_mode));
    ch_hash(high, low, &request->style.value_precision,
        sizeof(request->style.value_precision));
    ch_hash(high, low, &request->style.orientation,
        sizeof(request->style.orientation));
    ch_hash(high, low, &request->style.stack_mode,
        sizeof(request->style.stack_mode));
    ch_hash(high, low, &request->style.fill_opacity,
        sizeof(request->style.fill_opacity));
    ch_hash(high, low, &request->style.donut_inner_radius,
        sizeof(request->style.donut_inner_radius));
    ch_hash(high, low, &bounds.x, sizeof(bounds.x));
    ch_hash(high, low, &bounds.y, sizeof(bounds.y));
    ch_hash(high, low, &bounds.width, sizeof(bounds.width));
    ch_hash(high, low, &bounds.height, sizeof(bounds.height));
    ch_hash(high, low, &request->category_count,
        sizeof(request->category_count));
    ch_hash(high, low, &request->series_count,
        sizeof(request->series_count));
    ch_hash(high, low, &request->presentation_revision,
        sizeof(request->presentation_revision));
    for (i = 0u; i < request->category_count; ++i) {
        ch_hash_view(high, low, request->categories[i].id);
        ch_hash_view(high, low, request->categories[i].label);
    }
    for (i = 0u; i < request->series_count; ++i) {
        const fw_chart_series_v1 *series = &request->series[i];
        ch_hash_view(high, low, series->id);
        ch_hash_view(high, low, series->label);
        ch_hash(high, low, &series->color.red, sizeof(series->color.red));
        ch_hash(high, low, &series->color.green,
            sizeof(series->color.green));
        ch_hash(high, low, &series->color.blue, sizeof(series->color.blue));
        ch_hash(high, low, &series->color.alpha,
            sizeof(series->color.alpha));
        ch_hash(high, low, &series->visible, sizeof(series->visible));
        ch_hash(high, low, &series->mark, sizeof(series->mark));
        for (j = 0u; j < series->value_count; ++j) {
            ch_hash(high, low, &series->values[j].value,
                sizeof(series->values[j].value));
            ch_hash(high, low, &series->values[j].missing,
                sizeof(series->values[j].missing));
            ch_hash(high, low, &series->values[j].x,
                sizeof(series->values[j].x));
            ch_hash(high, low, &series->values[j].size,
                sizeof(series->values[j].size));
            ch_hash(high, low, &series->values[j].minimum,
                sizeof(series->values[j].minimum));
            ch_hash(high, low, &series->values[j].quartile1,
                sizeof(series->values[j].quartile1));
            ch_hash(high, low, &series->values[j].median,
                sizeof(series->values[j].median));
            ch_hash(high, low, &series->values[j].quartile3,
                sizeof(series->values[j].quartile3));
            ch_hash(high, low, &series->values[j].maximum,
                sizeof(series->values[j].maximum));
            ch_hash(high, low, &series->values[j].open,
                sizeof(series->values[j].open));
            ch_hash(high, low, &series->values[j].high,
                sizeof(series->values[j].high));
            ch_hash(high, low, &series->values[j].low,
                sizeof(series->values[j].low));
            ch_hash(high, low, &series->values[j].close,
                sizeof(series->values[j].close));
        }
    }
}

static fw_status FW_CALL ch_validate(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    fw_chart_validation_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    out_result->status = status;
    out_result->diagnostic_key = ch_view(key);
    return status;
}

static fw_status FW_CALL ch_measure(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    fw_chart_measure_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    out_result->intrinsic_size = ch_intrinsic(request);
    out_result->size = ch_measure_size(request);
    return FW_STATUS_OK;
}

static int ch_sink_valid(const fw_chart_draw_sink_v1 *sink) {
    return sink != NULL && sink->struct_size >= sizeof(*sink) &&
        sink->begin_chart != NULL && sink->end_chart != NULL &&
        sink->fill_rect != NULL && sink->stroke_line != NULL &&
        sink->fill_circle != NULL && sink->fill_sector != NULL &&
        sink->fill_polygon != NULL && sink->draw_label != NULL;
}

static int ch_find_series(const fw_chart_renderer_request_v1 *request,
    fw_string_view id, size_t *out_index) {
    size_t i;
    for (i = 0u; i < request->series_count; ++i) {
        if (ch_view_equal(id, request->series[i].id)) {
            if (out_index != NULL) *out_index = i;
            return 1;
        }
    }
    return 0;
}

static int ch_find_category(const fw_chart_renderer_request_v1 *request,
    fw_string_view id, size_t *out_index) {
    size_t i;
    for (i = 0u; i < request->category_count; ++i) {
        if (ch_view_equal(id, request->categories[i].id)) {
            if (out_index != NULL) *out_index = i;
            return 1;
        }
    }
    return 0;
}

static int ch_element_role_valid(fw_chart_element_role role,
    int selector) {
    return (selector && role == FW_CHART_ELEMENT_ROLE_ANY) ||
        (role >= FW_CHART_ELEMENT_ROLE_CHART_ROOT &&
         role <= FW_CHART_ELEMENT_ROLE_MAX);
}

static fw_chart_element_ref_v1 ch_element_ref(
    const fw_chart_renderer_request_v1 *request,
    fw_chart_element_role role, fw_string_view series_id,
    fw_string_view category_id, uint32_t part_index) {
    fw_chart_element_ref_v1 ref;
    memset(&ref, 0, sizeof(ref));
    ref.struct_size = sizeof(ref);
    ref.role = role;
    ref.chart_id = request->chart_id;
    ref.series_id = series_id;
    ref.category_id = category_id;
    ref.part_index = part_index;
    return ref;
}

static fw_chart_element_ref_v1 ch_element_parent(
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_ref_v1 *ref) {
    const fw_string_view empty = {NULL, 0u};
    if (ref->role == FW_CHART_ELEMENT_ROLE_CHART_ROOT) {
        fw_chart_element_ref_v1 none;
        memset(&none, 0, sizeof(none));
        none.struct_size = sizeof(none);
        none.part_index = 0u;
        return none;
    }
    if (ref->role == FW_CHART_ELEMENT_ROLE_GRID ||
        ref->role == FW_CHART_ELEMENT_ROLE_AXIS_X ||
        ref->role == FW_CHART_ELEMENT_ROLE_AXIS_Y ||
        ref->role == FW_CHART_ELEMENT_ROLE_CATEGORY_LABEL ||
        ref->role == FW_CHART_ELEMENT_ROLE_SERIES) {
        return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_PLOT_AREA,
            empty, empty, 0u);
    }
    if (ref->role == FW_CHART_ELEMENT_ROLE_DATUM) {
        return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_SERIES,
            ref->series_id, empty, 0u);
    }
    if (ref->role == FW_CHART_ELEMENT_ROLE_VALUE_LABEL) {
        return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_DATUM,
            ref->series_id, ref->category_id, ref->part_index);
    }
    if (ref->role == FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER)
        return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_CHART_ROOT,
            empty, empty, 0u);
    if (ref->role == FW_CHART_ELEMENT_ROLE_LEGEND_ITEM)
        return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER,
            empty, empty, 0u);
    if (ref->role == FW_CHART_ELEMENT_ROLE_LEGEND_MARKER ||
        ref->role == FW_CHART_ELEMENT_ROLE_LEGEND_LABEL ||
        ref->role == FW_CHART_ELEMENT_ROLE_LEGEND_VALUE)
        return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_LEGEND_ITEM,
            ref->series_id, empty, 0u);
    return ch_element_ref(request, FW_CHART_ELEMENT_ROLE_CHART_ROOT,
        empty, empty, 0u);
}

static const char *ch_element_role_name(fw_chart_element_role role) {
    switch (role) {
    case FW_CHART_ELEMENT_ROLE_CHART_ROOT: return "chart-root";
    case FW_CHART_ELEMENT_ROLE_PLOT_AREA: return "plot-area";
    case FW_CHART_ELEMENT_ROLE_GRID: return "grid";
    case FW_CHART_ELEMENT_ROLE_AXIS_X: return "axis-x";
    case FW_CHART_ELEMENT_ROLE_AXIS_Y: return "axis-y";
    case FW_CHART_ELEMENT_ROLE_TITLE: return "title";
    case FW_CHART_ELEMENT_ROLE_CATEGORY_LABEL: return "category-label";
    case FW_CHART_ELEMENT_ROLE_LEGEND_ITEM: return "legend-item";
    case FW_CHART_ELEMENT_ROLE_SERIES: return "series";
    case FW_CHART_ELEMENT_ROLE_DATUM: return "datum";
    case FW_CHART_ELEMENT_ROLE_VALUE_LABEL: return "value-label";
    case FW_CHART_ELEMENT_ROLE_ANNOTATION: return "annotation";
    case FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER: return "legend-container";
    case FW_CHART_ELEMENT_ROLE_LEGEND_MARKER: return "legend-marker";
    case FW_CHART_ELEMENT_ROLE_LEGEND_LABEL: return "legend-label";
    case FW_CHART_ELEMENT_ROLE_LEGEND_VALUE: return "legend-value";
    default: return NULL;
    }
}

static int ch_ref_shape_valid(const fw_chart_element_ref_v1 *ref,
    int selector) {
    return ref != NULL && ref->struct_size >= sizeof(*ref) &&
        ch_element_role_valid(ref->role, selector) &&
        ch_valid_string(ref->chart_id, !selector) &&
        ch_valid_string(ref->series_id, 0) &&
        ch_valid_string(ref->category_id, 0) &&
        (selector || ref->part_index != FW_CHART_ELEMENT_PART_ANY);
}

static int ch_ref_targets_model(
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_ref_v1 *ref) {
    if (ref->chart_id.length != 0u &&
        !ch_view_equal(ref->chart_id, request->chart_id)) return 0;
    if (ref->series_id.length != 0u &&
        !ch_find_series(request, ref->series_id, NULL)) return 0;
    if (ref->category_id.length != 0u &&
        !ch_find_category(request, ref->category_id, NULL)) return 0;
    return 1;
}

static fw_status ch_validate_element_overrides(
    fw_plugin_handle plugin, const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_override_v1 *overrides, size_t override_count,
    const char **out_key) {
    size_t i;
    fw_status status = ch_validate_request(plugin, request, out_key);
    if (status != FW_STATUS_OK) return status;
    if (override_count > CH_MAX_ELEMENT_OVERRIDES) {
        *out_key = "chart.elements.override_limit";
        return FW_STATUS_RESOURCE_LIMIT;
    }
    if (override_count != 0u && overrides == NULL) {
        *out_key = "chart.elements.missing_overrides";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    for (i = 0u; i < override_count; ++i) {
        const fw_chart_element_override_v1 *item = &overrides[i];
        if (item->struct_size < sizeof(*item) ||
            !ch_ref_shape_valid(&item->selector, 1) ||
            (item->fields & ~FW_CHART_OVERRIDE_ALL) != 0u ||
            item->fields == 0u) {
            *out_key = "chart.elements.invalid_override";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        if (!ch_ref_targets_model(request, &item->selector)) {
            *out_key = "chart.elements.selector_not_found";
            return FW_STATUS_NOT_FOUND;
        }
        if (((item->fields & FW_CHART_OVERRIDE_VISIBLE) != 0u &&
                !ch_bool(item->visible)) ||
            ((item->fields & FW_CHART_OVERRIDE_OPACITY) != 0u &&
                (!isfinite(item->opacity) || item->opacity < 0.0f ||
                 item->opacity > 1.0f)) ||
            ((item->fields & FW_CHART_OVERRIDE_COLOR) != 0u &&
                !ch_valid_color(item->color)) ||
            ((item->fields & FW_CHART_OVERRIDE_TRANSLATION) != 0u &&
                (!isfinite(item->translation.x) ||
                 !isfinite(item->translation.y))) ||
            ((item->fields & FW_CHART_OVERRIDE_SCALE) != 0u &&
                (!isfinite(item->uniform_scale) ||
                 item->uniform_scale <= 0.0f ||
                 item->uniform_scale > 100.0f)) ||
            ((item->fields & FW_CHART_OVERRIDE_ROTATION) != 0u &&
                !isfinite(item->rotation_radians)) ||
            ((item->fields & FW_CHART_OVERRIDE_ANCHOR) != 0u &&
                (!isfinite(item->anchor.x) || item->anchor.x < 0.0f ||
                 item->anchor.x > 1.0f || !isfinite(item->anchor.y) ||
                 item->anchor.y < 0.0f || item->anchor.y > 1.0f)) ||
            ((item->fields & FW_CHART_OVERRIDE_PROMOTION) != 0u &&
                item->promotion > FW_CHART_ELEMENT_PROMOTED)) {
            *out_key = "chart.elements.invalid_presentation";
            return FW_STATUS_INVALID_ARGUMENT;
        }
    }
    *out_key = "chart.elements.ok";
    return FW_STATUS_OK;
}

static fw_status FW_CALL ch_element_validate_overrides(
    fw_plugin_handle plugin, const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_override_v1 *overrides, size_t override_count,
    fw_chart_validation_result_v1 *out_result) {
    const char *key = "chart.elements.invalid_argument";
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_element_overrides(plugin, request, overrides,
        override_count, &key);
    out_result->status = status;
    out_result->diagnostic_key = ch_view(key);
    return status;
}

static int ch_unreserved(unsigned char value) {
    return (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == '-' || value == '.' ||
        value == '_' || value == '~';
}

static size_t ch_encoded_length(fw_string_view value) {
    size_t i;
    size_t length = 0u;
    for (i = 0u; i < value.length; ++i)
        length += ch_unreserved((unsigned char)value.data[i]) ? 1u : 3u;
    return length;
}

static void ch_write_encoded(char *buffer, size_t *offset,
    fw_string_view value) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    for (i = 0u; i < value.length; ++i) {
        const unsigned char byte = (unsigned char)value.data[i];
        if (ch_unreserved(byte)) buffer[(*offset)++] = (char)byte;
        else {
            buffer[(*offset)++] = '%';
            buffer[(*offset)++] = hex[byte >> 4u];
            buffer[(*offset)++] = hex[byte & 15u];
        }
    }
}

static fw_status FW_CALL ch_element_format_id(fw_plugin_handle plugin,
    const fw_chart_element_ref_v1 *ref, char *buffer, size_t capacity,
    size_t *out_required_length) {
    const char *role;
    size_t role_length;
    size_t required;
    size_t offset = 0u;
    char part[32];
    int part_length = 0;
    if (!ch_context_valid(plugin) || out_required_length == NULL ||
        !ch_ref_shape_valid(ref, 0)) return FW_STATUS_INVALID_ARGUMENT;
    role = ch_element_role_name(ref->role);
    if (role == NULL) return FW_STATUS_INVALID_ARGUMENT;
    role_length = strlen(role);
    if (ref->part_index != 0u)
        part_length = snprintf(part, sizeof(part), "%u", ref->part_index);
    required = 6u + ch_encoded_length(ref->chart_id) + 1u + role_length;
    if (ref->series_id.length != 0u)
        required += 1u + ch_encoded_length(ref->series_id);
    if (ref->category_id.length != 0u)
        required += 1u + ch_encoded_length(ref->category_id);
    if (part_length > 0) required += 1u + (size_t)part_length;
    *out_required_length = required;
    if (buffer == NULL || capacity <= required)
        return FW_STATUS_BUFFER_TOO_SMALL;
    memcpy(buffer + offset, "chart/", 6u); offset += 6u;
    ch_write_encoded(buffer, &offset, ref->chart_id);
    buffer[offset++] = '/';
    memcpy(buffer + offset, role, role_length); offset += role_length;
    if (ref->series_id.length != 0u) {
        buffer[offset++] = '/';
        ch_write_encoded(buffer, &offset, ref->series_id);
    }
    if (ref->category_id.length != 0u) {
        buffer[offset++] = '/';
        ch_write_encoded(buffer, &offset, ref->category_id);
    }
    if (part_length > 0) {
        buffer[offset++] = '/';
        memcpy(buffer + offset, part, (size_t)part_length);
        offset += (size_t)part_length;
    }
    buffer[offset] = '\0';
    return FW_STATUS_OK;
}

static fw_chart_element_capabilities ch_element_capabilities(
    fw_chart_element_role role) {
    fw_chart_element_capabilities value = FW_CHART_ELEMENT_CAN_HIDE |
        FW_CHART_ELEMENT_CAN_OPACITY | FW_CHART_ELEMENT_CAN_TRANSFORM |
        FW_CHART_ELEMENT_CAN_REORDER;
    if (role != FW_CHART_ELEMENT_ROLE_CHART_ROOT)
        value |= FW_CHART_ELEMENT_CAN_PROMOTE;
    if (role != FW_CHART_ELEMENT_ROLE_PLOT_AREA)
        value |= FW_CHART_ELEMENT_CAN_COLOR;
    if (role == FW_CHART_ELEMENT_ROLE_SERIES ||
        role == FW_CHART_ELEMENT_ROLE_DATUM ||
        role == FW_CHART_ELEMENT_ROLE_VALUE_LABEL ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_ITEM ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_MARKER ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_LABEL ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_VALUE)
        value |= FW_CHART_ELEMENT_DATA_BOUND;
    return value;
}

static int32_t ch_element_z(fw_chart_element_role role) {
    switch (role) {
    case FW_CHART_ELEMENT_ROLE_CHART_ROOT: return 0;
    case FW_CHART_ELEMENT_ROLE_PLOT_AREA: return 10;
    case FW_CHART_ELEMENT_ROLE_GRID: return 20;
    case FW_CHART_ELEMENT_ROLE_AXIS_X:
    case FW_CHART_ELEMENT_ROLE_AXIS_Y: return 30;
    case FW_CHART_ELEMENT_ROLE_SERIES: return 40;
    case FW_CHART_ELEMENT_ROLE_DATUM: return 50;
    case FW_CHART_ELEMENT_ROLE_CATEGORY_LABEL:
    case FW_CHART_ELEMENT_ROLE_VALUE_LABEL: return 60;
    case FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER: return 68;
    case FW_CHART_ELEMENT_ROLE_LEGEND_ITEM: return 70;
    case FW_CHART_ELEMENT_ROLE_LEGEND_MARKER:
    case FW_CHART_ELEMENT_ROLE_LEGEND_LABEL:
    case FW_CHART_ELEMENT_ROLE_LEGEND_VALUE: return 71;
    case FW_CHART_ELEMENT_ROLE_TITLE: return 80;
    default: return 90;
    }
}

static fw_string_view ch_element_label(
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_ref_v1 *ref) {
    size_t index;
    if (ref->role == FW_CHART_ELEMENT_ROLE_CHART_ROOT ||
        ref->role == FW_CHART_ELEMENT_ROLE_TITLE)
        return request->title.length != 0u ? request->title :
            request->chart_id;
    if (ref->category_id.length != 0u &&
        ch_find_category(request, ref->category_id, &index))
        return request->categories[index].label.length != 0u ?
            request->categories[index].label : request->categories[index].id;
    if (ref->series_id.length != 0u &&
        ch_find_series(request, ref->series_id, &index))
        return request->series[index].label.length != 0u ?
            request->series[index].label : request->series[index].id;
    return (fw_string_view){ch_element_role_name(ref->role),
        strlen(ch_element_role_name(ref->role))};
}

static fw_chart_element_descriptor_v1 ch_element_descriptor(
    const fw_chart_renderer_request_v1 *request,
    fw_chart_element_role role, fw_string_view series_id,
    fw_string_view category_id, uint32_t part_index,
    fw_rect_f32 bounds, uint32_t flags) {
    fw_chart_element_descriptor_v1 result;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.ref = ch_element_ref(request, role, series_id, category_id,
        part_index);
    result.parent = ch_element_parent(request, &result.ref);
    result.normalized_bounds = bounds;
    result.z_index = ch_element_z(role);
    result.capabilities = ch_element_capabilities(role);
    result.label = ch_element_label(request, &result.ref);
    result.flags = flags;
    return result;
}

static fw_rect_f32 ch_datum_bounds(
    const fw_chart_renderer_request_v1 *request, const ch_plot *plot,
    size_t series_index, size_t category_index, uint32_t *out_flags) {
    const fw_chart_series_v1 *series = &request->series[series_index];
    const fw_chart_value_v1 *value = &series->values[category_index];
    const float group = plot->width / (float)request->category_count;
    const float center_x = plot->x + group *
        ((float)category_index + 0.5f);
    fw_rect_f32 result = {plot->x, plot->y, plot->width, plot->height};
    *out_flags = FW_CHART_ELEMENT_BOUNDS_APPROXIMATE;
    if ((request->kind == FW_CHART_BAR ||
         request->kind == FW_CHART_HISTOGRAM) &&
        request->style.orientation == FW_CHART_ORIENTATION_VERTICAL &&
        request->style.stack_mode == FW_CHART_STACK_NONE) {
        const float width = group * (1.0f - request->style.bar_gap_ratio) /
            (float)plot->visible_series;
        const float y = ch_value_y(plot, value->value);
        const float zero = ch_value_y(plot, 0.0);
        uint32_t visible = 0u;
        size_t i;
        for (i = 0u; i < series_index; ++i)
            if (request->series[i].visible != 0u) ++visible;
        result.x = plot->x + group * (float)category_index +
            (group - group * (1.0f - request->style.bar_gap_ratio)) * 0.5f +
            width * (float)visible;
        result.y = y < zero ? y : zero;
        result.width = width;
        result.height = fabsf(y - zero);
        *out_flags = 0u;
    } else if (request->kind == FW_CHART_LINE ||
        request->kind == FW_CHART_SCATTER ||
        request->kind == FW_CHART_BUBBLE ||
        request->kind == FW_CHART_TIME_SERIES ||
        request->kind == FW_CHART_COMBO) {
        const float radius = request->kind == FW_CHART_BUBBLE ? 0.05f :
            fmaxf(request->style.point_radius, 0.02f);
        result = (fw_rect_f32){center_x - radius,
            ch_value_y(plot, value->value) - radius,
            radius * 2.0f, radius * 2.0f};
    } else if (request->kind == FW_CHART_PIE ||
        request->kind == FW_CHART_DONUT ||
        request->kind == FW_CHART_GAUGE ||
        request->kind == FW_CHART_RADAR) {
        result = (fw_rect_f32){0.16f, 0.12f, 0.68f, 0.68f};
    } else if (request->kind == FW_CHART_HEATMAP) {
        uint32_t visible = 0u;
        size_t i;
        for (i = 0u; i < series_index; ++i)
            if (request->series[i].visible != 0u) ++visible;
        result = (fw_rect_f32){plot->x + plot->width *
            (float)category_index / (float)request->category_count,
            plot->y + plot->height * (float)visible /
                (float)plot->visible_series,
            plot->width / (float)request->category_count,
            plot->height / (float)plot->visible_series};
        *out_flags = 0u;
    } else if (request->kind == FW_CHART_FUNNEL) {
        result = (fw_rect_f32){0.14f,
            0.16f + 0.66f * (float)category_index /
                (float)request->category_count,
            0.72f, 0.66f / (float)request->category_count};
    }
    return result;
}

static fw_status ch_visit_element(
    const fw_chart_element_enum_sink_v1 *sink,
    fw_chart_element_enum_result_v1 *result,
    const fw_chart_element_descriptor_v1 *descriptor,
    uint32_t limit) {
    fw_status status;
    if (result->emitted_element_count >= limit)
        return FW_STATUS_RESOURCE_LIMIT;
    status = sink->visit(sink->user_data, descriptor);
    if (status != FW_STATUS_OK) return FW_STATUS_SINK_REJECTED;
    ++result->emitted_element_count;
    if ((descriptor->capabilities & FW_CHART_ELEMENT_CAN_PROMOTE) != 0u)
        ++result->promotable_element_count;
    if ((descriptor->flags & FW_CHART_ELEMENT_VIRTUALIZED) != 0u)
        ++result->virtualized_element_count;
    return FW_STATUS_OK;
}

static fw_status FW_CALL ch_element_enumerate(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_enum_sink_v1 *sink,
    fw_chart_element_enum_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    uint32_t limit;
    size_t series;
    size_t category;
    ch_plot plot;
    const fw_string_view empty = {NULL, 0u};
    fw_chart_element_descriptor_v1 descriptor;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        sink == NULL || sink->struct_size < sizeof(*sink) ||
        sink->visit == NULL) return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    limit = ch_resolve_limits(&request->budget).commands;
    plot = ch_make_plot(request);
#define CH_VISIT(role_, series_, category_, bounds_, flags_) do { \
    descriptor = ch_element_descriptor(request, (role_), (series_), \
        (category_), 0u, (bounds_), (flags_)); \
    status = ch_visit_element(sink, out_result, &descriptor, limit); \
    if (status != FW_STATUS_OK) return status; \
} while (0)
    CH_VISIT(FW_CHART_ELEMENT_ROLE_CHART_ROOT, empty, empty,
        ((fw_rect_f32){0.0f, 0.0f, 1.0f, 1.0f}), 0u);
    CH_VISIT(FW_CHART_ELEMENT_ROLE_PLOT_AREA, empty, empty,
        ((fw_rect_f32){plot.x, plot.y, plot.width, plot.height}), 0u);
    if (request->style.show_grid != 0u)
        CH_VISIT(FW_CHART_ELEMENT_ROLE_GRID, empty, empty,
            ((fw_rect_f32){plot.x, plot.y, plot.width, plot.height}), 0u);
    if (request->style.show_axes != 0u && ch_cartesian_guides(request->kind)) {
        CH_VISIT(FW_CHART_ELEMENT_ROLE_AXIS_X, empty, empty,
            ((fw_rect_f32){plot.x, plot.y + plot.height,
                plot.width, 0.002f}), 0u);
        CH_VISIT(FW_CHART_ELEMENT_ROLE_AXIS_Y, empty, empty,
            ((fw_rect_f32){plot.x, plot.y, 0.002f, plot.height}), 0u);
    }
    if (request->style.show_labels != 0u && request->title.length != 0u)
        CH_VISIT(FW_CHART_ELEMENT_ROLE_TITLE, empty, empty,
            ((fw_rect_f32){0.12f, 0.01f, 0.76f, 0.08f}),
            FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    if (request->style.show_labels != 0u) {
        for (category = 0u; category < request->category_count; ++category) {
            const float x = plot.x + plot.width *
                ((float)category + 0.5f) /
                (float)request->category_count;
            CH_VISIT(FW_CHART_ELEMENT_ROLE_CATEGORY_LABEL, empty,
                request->categories[category].id,
                ((fw_rect_f32){x - 0.06f, 0.85f, 0.12f, 0.06f}),
                FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
        }
    }
    if (request->style.show_legend != 0u) {
        const float slot_width = 0.76f / (float)plot.visible_series;
        uint32_t visible_index = 0u;
        CH_VISIT(FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER, empty, empty,
            ((fw_rect_f32){0.10f, 0.91f, 0.80f, 0.08f}),
            FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
        for (series = 0u; series < request->series_count; ++series) {
            float x;
            fw_rect_f32 item_bounds;
            if (request->series[series].visible == 0u) continue;
            x = 0.12f + (float)visible_index * slot_width;
            item_bounds = (fw_rect_f32){x, 0.92f, slot_width, 0.07f};
            CH_VISIT(FW_CHART_ELEMENT_ROLE_LEGEND_ITEM,
                request->series[series].id, empty,
                item_bounds,
                FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
            CH_VISIT(FW_CHART_ELEMENT_ROLE_LEGEND_MARKER,
                request->series[series].id, empty,
                ((fw_rect_f32){x + 0.006f, 0.942f, 0.018f, 0.018f}),
                FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
            CH_VISIT(FW_CHART_ELEMENT_ROLE_LEGEND_LABEL,
                request->series[series].id, empty,
                ((fw_rect_f32){x + 0.030f, 0.92f,
                    fmaxf(0.0f, slot_width - 0.030f), 0.07f}),
                FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
            ++visible_index;
        }
    }
    for (series = 0u; series < request->series_count; ++series) {
        if (request->series[series].visible == 0u) continue;
        CH_VISIT(FW_CHART_ELEMENT_ROLE_SERIES, request->series[series].id,
            empty, ((fw_rect_f32){plot.x, plot.y, plot.width, plot.height}),
            FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
        for (category = 0u; category < request->category_count; ++category) {
            fw_rect_f32 datum_bounds;
            uint32_t datum_flags;
            if (request->series[series].values[category].missing != 0u)
                continue;
            datum_bounds = ch_datum_bounds(request, &plot, series,
                category, &datum_flags);
            CH_VISIT(FW_CHART_ELEMENT_ROLE_DATUM,
                request->series[series].id,
                request->categories[category].id,
                datum_bounds, datum_flags);
            if (request->style.show_value_labels != 0u)
                CH_VISIT(FW_CHART_ELEMENT_ROLE_VALUE_LABEL,
                    request->series[series].id,
                    request->categories[category].id,
                    datum_bounds,
                    datum_flags | FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
        }
    }
#undef CH_VISIT
    return FW_STATUS_OK;
}

typedef struct ch_layer_sink_context {
    const fw_chart_renderer_request_v1 *request;
    const fw_chart_draw_sink_v1 *downstream;
    const fw_chart_element_override_v1 *overrides;
    size_t override_count;
    const fw_chart_element_observer_v1 *observer;
    fw_chart_element_descriptor_v1 current;
    fw_chart_element_presentation_v1 presentation;
    uint32_t has_current;
} ch_layer_sink_context;

static int ch_legend_child_role(fw_chart_element_role role) {
    return role == FW_CHART_ELEMENT_ROLE_LEGEND_ITEM ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_MARKER ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_LABEL ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_VALUE;
}

static int ch_legend_item_part_role(fw_chart_element_role role) {
    return role == FW_CHART_ELEMENT_ROLE_LEGEND_MARKER ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_LABEL ||
        role == FW_CHART_ELEMENT_ROLE_LEGEND_VALUE;
}

static fw_rect_f32 ch_legend_item_bounds_for_series(
    const fw_chart_renderer_request_v1 *request,
    fw_string_view series_id) {
    size_t index;
    size_t current;
    uint32_t visible_index = 0u;
    const uint32_t visible_count = ch_visible_series(request);
    if (visible_count == 0u || !ch_find_series(request, series_id, &index))
        return (fw_rect_f32){0.10f, 0.91f, 0.80f, 0.08f};
    for (current = 0u; current < index; ++current)
        if (request->series[current].visible != 0u) ++visible_index;
    return (fw_rect_f32){0.12f + 0.76f * (float)visible_index /
            (float)visible_count, 0.92f,
        0.76f / (float)visible_count, 0.07f};
}

static int ch_override_matches(const fw_chart_element_ref_v1 *selector,
    const fw_chart_element_ref_v1 *current) {
    int role_match = 0;
    if (selector->chart_id.length != 0u &&
        !ch_view_equal(selector->chart_id, current->chart_id)) return 0;
    if (selector->series_id.length != 0u &&
        !ch_view_equal(selector->series_id, current->series_id)) return 0;
    if (selector->category_id.length != 0u &&
        !ch_view_equal(selector->category_id, current->category_id)) return 0;
    if (selector->part_index != FW_CHART_ELEMENT_PART_ANY &&
        selector->part_index != current->part_index) return 0;
    if (selector->role == FW_CHART_ELEMENT_ROLE_ANY ||
        selector->role == FW_CHART_ELEMENT_ROLE_CHART_ROOT ||
        selector->role == current->role) role_match = 1;
    else if (selector->role == FW_CHART_ELEMENT_ROLE_SERIES &&
        (current->role == FW_CHART_ELEMENT_ROLE_DATUM ||
         current->role == FW_CHART_ELEMENT_ROLE_VALUE_LABEL ||
         ch_legend_child_role(current->role))) role_match = 1;
    else if (selector->role == FW_CHART_ELEMENT_ROLE_DATUM &&
        current->role == FW_CHART_ELEMENT_ROLE_VALUE_LABEL) role_match = 1;
    else if (selector->role == FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER &&
        ch_legend_child_role(current->role)) role_match = 1;
    else if (selector->role == FW_CHART_ELEMENT_ROLE_LEGEND_ITEM &&
        ch_legend_item_part_role(current->role)) role_match = 1;
    return role_match;
}

static fw_chart_element_presentation_v1 ch_effective_presentation(
    const ch_layer_sink_context *context,
    const fw_chart_element_descriptor_v1 *descriptor) {
    fw_chart_element_presentation_v1 result;
    size_t i;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.visible = 1u;
    result.opacity = 1.0f;
    result.uniform_scale = 1.0f;
    result.anchor = (fw_point_f32){
        descriptor->normalized_bounds.x +
            descriptor->normalized_bounds.width * 0.5f,
        descriptor->normalized_bounds.y +
            descriptor->normalized_bounds.height * 0.5f};
    result.promotion = FW_CHART_ELEMENT_INLINE;
    for (i = 0u; i < context->override_count; ++i) {
        const fw_chart_element_override_v1 *item = &context->overrides[i];
        if (!ch_override_matches(&item->selector, &descriptor->ref))
            continue;
        if ((item->fields & (FW_CHART_OVERRIDE_SCALE |
                FW_CHART_OVERRIDE_ROTATION)) != 0u &&
            (item->fields & FW_CHART_OVERRIDE_ANCHOR) == 0u) {
            fw_rect_f32 group_bounds = descriptor->normalized_bounds;
            if (item->selector.role ==
                FW_CHART_ELEMENT_ROLE_LEGEND_CONTAINER &&
                ch_legend_child_role(descriptor->ref.role))
                group_bounds = (fw_rect_f32){0.10f, 0.91f, 0.80f, 0.08f};
            else if (item->selector.role ==
                FW_CHART_ELEMENT_ROLE_LEGEND_ITEM &&
                ch_legend_item_part_role(descriptor->ref.role))
                group_bounds = ch_legend_item_bounds_for_series(
                    context->request, descriptor->ref.series_id);
            result.anchor = (fw_point_f32){group_bounds.x +
                    group_bounds.width * 0.5f,
                group_bounds.y + group_bounds.height * 0.5f};
        }
        if ((item->fields & FW_CHART_OVERRIDE_VISIBLE) != 0u)
            result.visible = item->visible;
        if ((item->fields & FW_CHART_OVERRIDE_OPACITY) != 0u)
            result.opacity = item->opacity;
        if ((item->fields & FW_CHART_OVERRIDE_COLOR) != 0u) {
            result.color = item->color;
            result.has_color_override = 1u;
        }
        if ((item->fields & FW_CHART_OVERRIDE_TRANSLATION) != 0u)
            result.translation = item->translation;
        if ((item->fields & FW_CHART_OVERRIDE_SCALE) != 0u)
            result.uniform_scale = item->uniform_scale;
        if ((item->fields & FW_CHART_OVERRIDE_ROTATION) != 0u)
            result.rotation_radians = item->rotation_radians;
        if ((item->fields & FW_CHART_OVERRIDE_ANCHOR) != 0u) {
            result.anchor = item->anchor;
            result.has_explicit_anchor = 1u;
        }
        if ((item->fields & FW_CHART_OVERRIDE_Z_OFFSET) != 0u)
            result.z_offset = item->z_offset;
        if ((item->fields & FW_CHART_OVERRIDE_PROMOTION) != 0u)
            result.promotion = item->promotion;
    }
    return result;
}

static fw_status ch_layer_select(ch_layer_sink_context *context,
    fw_chart_element_role role, fw_string_view series_id,
    fw_string_view category_id, uint32_t part_index,
    fw_rect_f32 bounds, uint32_t flags) {
    fw_status status = FW_STATUS_OK;
    context->current = ch_element_descriptor(context->request, role,
        series_id, category_id, part_index, bounds, flags);
    context->presentation = ch_effective_presentation(context,
        &context->current);
    context->has_current = 1u;
    if (context->observer != NULL)
        status = context->observer->select(context->observer->user_data,
            &context->current, &context->presentation);
    return status;
}

static fw_status ch_layer_select_data(ch_layer_sink_context *context,
    fw_string_view series_id, fw_string_view category_id,
    fw_rect_f32 bounds, uint32_t flags) {
    const fw_string_view empty = {NULL, 0u};
    if (series_id.length != 0u && category_id.length != 0u)
        return ch_layer_select(context, FW_CHART_ELEMENT_ROLE_DATUM,
            series_id, category_id, 0u, bounds, flags);
    if (series_id.length != 0u) {
        if (context->has_current != 0u &&
            context->current.ref.role == FW_CHART_ELEMENT_ROLE_DATUM &&
            ch_view_equal(context->current.ref.series_id, series_id))
            return ch_layer_select(context, FW_CHART_ELEMENT_ROLE_DATUM,
                series_id, context->current.ref.category_id, 0u, bounds,
                flags | FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
        return ch_layer_select(context, FW_CHART_ELEMENT_ROLE_SERIES,
            series_id, empty, 0u, bounds,
            flags | FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    }
    return ch_layer_select(context, FW_CHART_ELEMENT_ROLE_PLOT_AREA,
        empty, empty, 0u, bounds, flags);
}

static fw_point_f32 ch_layer_point(
    const fw_chart_element_presentation_v1 *presentation,
    fw_point_f32 point) {
    const float dx = (point.x - presentation->anchor.x) *
        presentation->uniform_scale;
    const float dy = (point.y - presentation->anchor.y) *
        presentation->uniform_scale;
    const float cosine = cosf(presentation->rotation_radians);
    const float sine = sinf(presentation->rotation_radians);
    return (fw_point_f32){presentation->anchor.x + dx * cosine - dy * sine +
            presentation->translation.x,
        presentation->anchor.y + dx * sine + dy * cosine +
            presentation->translation.y};
}

static fw_color_rgba_f32 ch_layer_color(
    const fw_chart_element_presentation_v1 *presentation,
    fw_color_rgba_f32 source) {
    fw_color_rgba_f32 result = source;
    if (presentation->has_color_override != 0u)
        result = presentation->color;
    result.alpha *= presentation->opacity;
    return result;
}

static fw_rect_f32 ch_points_bounds(const fw_point_f32 *points,
    size_t count) {
    size_t i;
    fw_rect_f32 result = {points[0].x, points[0].y, 0.0f, 0.0f};
    float maximum_x = points[0].x;
    float maximum_y = points[0].y;
    for (i = 1u; i < count; ++i) {
        if (points[i].x < result.x) result.x = points[i].x;
        if (points[i].y < result.y) result.y = points[i].y;
        if (points[i].x > maximum_x) maximum_x = points[i].x;
        if (points[i].y > maximum_y) maximum_y = points[i].y;
    }
    result.width = maximum_x - result.x;
    result.height = maximum_y - result.y;
    return result;
}

static fw_status FW_CALL ch_layer_begin(void *user_data,
    const fw_visual_transform_result_v1 *transform, float opacity) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    return context->downstream->begin_chart(context->downstream->user_data,
        transform, opacity);
}

static fw_status FW_CALL ch_layer_end(void *user_data) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    return context->downstream->end_chart(context->downstream->user_data);
}

static fw_status FW_CALL ch_layer_rect(void *user_data, fw_rect_f32 rect,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    fw_status status;
    fw_point_f32 points[4];
    size_t i;
    if (series_id.length != 0u &&
        ((category_id.length == 0u && rect.y >= 0.90f) ||
         ch_view_equal(category_id, ch_view(CH_LEGEND_MARKER_TAG))))
        status = ch_layer_select(context,
            FW_CHART_ELEMENT_ROLE_LEGEND_MARKER, series_id,
            (fw_string_view){NULL, 0u},
            0u, rect, FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    else status = ch_layer_select_data(context, series_id, category_id,
        rect, 0u);
    if (status != FW_STATUS_OK) return status;
    if (context->presentation.visible == 0u ||
        context->presentation.opacity == 0.0f) return FW_STATUS_OK;
    color = ch_layer_color(&context->presentation, color);
    points[0] = (fw_point_f32){rect.x, rect.y};
    points[1] = (fw_point_f32){rect.x + rect.width, rect.y};
    points[2] = (fw_point_f32){rect.x + rect.width, rect.y + rect.height};
    points[3] = (fw_point_f32){rect.x, rect.y + rect.height};
    for (i = 0u; i < 4u; ++i)
        points[i] = ch_layer_point(&context->presentation, points[i]);
    if (fabsf(context->presentation.rotation_radians) <= 0.000001f) {
        fw_rect_f32 transformed = {points[0].x, points[0].y,
            points[2].x - points[0].x, points[2].y - points[0].y};
        return context->downstream->fill_rect(
            context->downstream->user_data, transformed, color,
            series_id, ch_view_equal(category_id,
                ch_view(CH_LEGEND_MARKER_TAG)) ?
                    (fw_string_view){NULL, 0u} : category_id);
    }
    return context->downstream->fill_polygon(
        context->downstream->user_data, points, 4u, color,
        series_id, ch_view_equal(category_id,
            ch_view(CH_LEGEND_MARKER_TAG)) ?
                (fw_string_view){NULL, 0u} : category_id);
}

static fw_status FW_CALL ch_layer_line(void *user_data,
    fw_point_f32 start, fw_point_f32 end, float width,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    fw_rect_f32 bounds = {fminf(start.x, end.x), fminf(start.y, end.y),
        fabsf(end.x - start.x), fabsf(end.y - start.y)};
    fw_status status;
    const fw_string_view empty = {NULL, 0u};
    if (series_id.length == 0u && category_id.length == 0u) {
        fw_chart_element_role role;
        if (width <= 0.0016f) role = FW_CHART_ELEMENT_ROLE_GRID;
        else if (fabsf(start.x - end.x) <= 0.0001f)
            role = FW_CHART_ELEMENT_ROLE_AXIS_Y;
        else role = FW_CHART_ELEMENT_ROLE_AXIS_X;
        status = ch_layer_select(context, role, empty, empty, 0u,
            bounds, 0u);
    } else status = ch_layer_select_data(context, series_id, category_id,
        bounds, FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    if (status != FW_STATUS_OK) return status;
    if (context->presentation.visible == 0u ||
        context->presentation.opacity == 0.0f) return FW_STATUS_OK;
    return context->downstream->stroke_line(
        context->downstream->user_data,
        ch_layer_point(&context->presentation, start),
        ch_layer_point(&context->presentation, end),
        width * context->presentation.uniform_scale,
        ch_layer_color(&context->presentation, color),
        series_id, category_id);
}

static fw_status FW_CALL ch_layer_circle(void *user_data,
    fw_point_f32 center, float radius, fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    const fw_rect_f32 bounds = {center.x - radius, center.y - radius,
        radius * 2.0f, radius * 2.0f};
    fw_status status = ch_layer_select_data(context, series_id, category_id,
        bounds, 0u);
    if (status != FW_STATUS_OK) return status;
    if (context->presentation.visible == 0u ||
        context->presentation.opacity == 0.0f) return FW_STATUS_OK;
    return context->downstream->fill_circle(
        context->downstream->user_data,
        ch_layer_point(&context->presentation, center),
        radius * context->presentation.uniform_scale,
        ch_layer_color(&context->presentation, color),
        series_id, category_id);
}

static fw_status FW_CALL ch_layer_sector(void *user_data,
    fw_point_f32 center, float outer_radius, float inner_radius,
    float start, float sweep, fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    const fw_rect_f32 bounds = {center.x - outer_radius,
        center.y - outer_radius, outer_radius * 2.0f,
        outer_radius * 2.0f};
    fw_status status = ch_layer_select_data(context, series_id, category_id,
        bounds, FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    if (status != FW_STATUS_OK) return status;
    if (context->presentation.visible == 0u ||
        context->presentation.opacity == 0.0f) return FW_STATUS_OK;
    return context->downstream->fill_sector(
        context->downstream->user_data,
        ch_layer_point(&context->presentation, center),
        outer_radius * context->presentation.uniform_scale,
        inner_radius * context->presentation.uniform_scale,
        start + context->presentation.rotation_radians, sweep,
        ch_layer_color(&context->presentation, color),
        series_id, category_id);
}

static fw_status FW_CALL ch_layer_polygon(void *user_data,
    const fw_point_f32 *points, size_t point_count,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    fw_point_f32 transformed[CH_LAYER_MAX_POLYGON_POINTS];
    fw_rect_f32 bounds;
    fw_status status;
    size_t i;
    if (points == NULL || point_count == 0u ||
        point_count > CH_LAYER_MAX_POLYGON_POINTS)
        return FW_STATUS_RESOURCE_LIMIT;
    bounds = ch_points_bounds(points, point_count);
    status = ch_layer_select_data(context, series_id, category_id,
        bounds, FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    if (status != FW_STATUS_OK) return status;
    if (context->presentation.visible == 0u ||
        context->presentation.opacity == 0.0f) return FW_STATUS_OK;
    for (i = 0u; i < point_count; ++i)
        transformed[i] = ch_layer_point(&context->presentation, points[i]);
    return context->downstream->fill_polygon(
        context->downstream->user_data, transformed, point_count,
        ch_layer_color(&context->presentation, color),
        series_id, category_id);
}

static fw_chart_element_role ch_label_role(
    ch_layer_sink_context *context, fw_string_view text,
    fw_string_view element_id, fw_string_view *out_series,
    fw_string_view *out_category) {
    size_t index;
    const fw_string_view empty = {NULL, 0u};
    *out_series = empty;
    *out_category = empty;
    if (ch_view_equal(element_id, context->request->chart_id))
        return FW_CHART_ELEMENT_ROLE_TITLE;
    if (ch_find_category(context->request, element_id, &index)) {
        const fw_string_view label =
            context->request->categories[index].label.length != 0u ?
            context->request->categories[index].label :
            context->request->categories[index].id;
        *out_category = context->request->categories[index].id;
        if (ch_view_equal(text, label))
            return FW_CHART_ELEMENT_ROLE_CATEGORY_LABEL;
        if (context->has_current != 0u &&
            context->current.ref.role == FW_CHART_ELEMENT_ROLE_DATUM) {
            *out_series = context->current.ref.series_id;
        } else if (context->request->series_count != 0u) {
            *out_series = context->request->series[0].id;
        }
        return FW_CHART_ELEMENT_ROLE_VALUE_LABEL;
    }
    if (ch_find_series(context->request, element_id, &index)) {
        const fw_string_view label =
            context->request->series[index].label.length != 0u ?
            context->request->series[index].label :
            context->request->series[index].id;
        *out_series = context->request->series[index].id;
        if (context->has_current != 0u &&
            context->current.ref.role == FW_CHART_ELEMENT_ROLE_DATUM &&
            ch_view_equal(context->current.ref.series_id, *out_series)) {
            *out_category = context->current.ref.category_id;
            return FW_CHART_ELEMENT_ROLE_VALUE_LABEL;
        }
        return ch_view_equal(text, label) ?
            FW_CHART_ELEMENT_ROLE_LEGEND_LABEL :
            FW_CHART_ELEMENT_ROLE_SERIES;
    }
    return FW_CHART_ELEMENT_ROLE_ANNOTATION;
}

static fw_status FW_CALL ch_layer_label(void *user_data,
    fw_string_view text, fw_point_f32 anchor, float font_size,
    fw_color_rgba_f32 color, fw_string_view element_id) {
    ch_layer_sink_context *context = (ch_layer_sink_context *)user_data;
    fw_string_view series_id;
    fw_string_view category_id;
    const fw_chart_element_role role = ch_label_role(context, text,
        element_id, &series_id, &category_id);
    const fw_rect_f32 bounds = {anchor.x - font_size * 2.0f,
        anchor.y - font_size, font_size * 4.0f, font_size * 2.0f};
    fw_status status = ch_layer_select(context, role, series_id,
        category_id, 0u, bounds, FW_CHART_ELEMENT_BOUNDS_APPROXIMATE);
    if (status != FW_STATUS_OK) return status;
    if (context->presentation.visible == 0u ||
        context->presentation.opacity == 0.0f) return FW_STATUS_OK;
    return context->downstream->draw_label(
        context->downstream->user_data, text,
        ch_layer_point(&context->presentation, anchor),
        font_size * context->presentation.uniform_scale,
        ch_layer_color(&context->presentation, color), element_id);
}

static fw_chart_draw_sink_v1 ch_layer_sink(
    ch_layer_sink_context *context) {
    fw_chart_draw_sink_v1 sink;
    sink = (fw_chart_draw_sink_v1){sizeof(sink), context,
        ch_layer_begin, ch_layer_end, ch_layer_rect, ch_layer_line,
        ch_layer_circle, ch_layer_sector, ch_layer_polygon,
        ch_layer_label};
    return sink;
}

static void ch_hash_element_overrides(uint64_t *high, uint64_t *low,
    const fw_chart_element_override_v1 *overrides,
    size_t override_count) {
    size_t i;
    ch_hash(high, low, &override_count, sizeof(override_count));
    for (i = 0u; i < override_count; ++i) {
        const fw_chart_element_override_v1 *item = &overrides[i];
        ch_hash(high, low, &item->selector.role,
            sizeof(item->selector.role));
        ch_hash_view(high, low, item->selector.chart_id);
        ch_hash_view(high, low, item->selector.series_id);
        ch_hash_view(high, low, item->selector.category_id);
        ch_hash(high, low, &item->selector.part_index,
            sizeof(item->selector.part_index));
        ch_hash(high, low, &item->fields, sizeof(item->fields));
        if ((item->fields & FW_CHART_OVERRIDE_VISIBLE) != 0u)
            ch_hash(high, low, &item->visible, sizeof(item->visible));
        if ((item->fields & FW_CHART_OVERRIDE_OPACITY) != 0u)
            ch_hash(high, low, &item->opacity, sizeof(item->opacity));
        if ((item->fields & FW_CHART_OVERRIDE_COLOR) != 0u)
            ch_hash(high, low, &item->color, sizeof(item->color));
        if ((item->fields & FW_CHART_OVERRIDE_TRANSLATION) != 0u)
            ch_hash(high, low, &item->translation,
                sizeof(item->translation));
        if ((item->fields & FW_CHART_OVERRIDE_SCALE) != 0u)
            ch_hash(high, low, &item->uniform_scale,
                sizeof(item->uniform_scale));
        if ((item->fields & FW_CHART_OVERRIDE_ROTATION) != 0u)
            ch_hash(high, low, &item->rotation_radians,
                sizeof(item->rotation_radians));
        if ((item->fields & FW_CHART_OVERRIDE_ANCHOR) != 0u)
            ch_hash(high, low, &item->anchor, sizeof(item->anchor));
        if ((item->fields & FW_CHART_OVERRIDE_Z_OFFSET) != 0u)
            ch_hash(high, low, &item->z_offset, sizeof(item->z_offset));
        if ((item->fields & FW_CHART_OVERRIDE_PROMOTION) != 0u)
            ch_hash(high, low, &item->promotion,
                sizeof(item->promotion));
    }
}

static fw_status FW_CALL ch_render(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request, fw_rect_f32 bounds,
    const fw_chart_services_v1 *services,
    fw_chart_render_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    fw_visual_transform_result_v1 transform;
    ch_emitter emitter;
    ch_plot plot;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(*out_result) ||
        services == NULL || services->struct_size < sizeof(*services) ||
        !ch_valid_rect(bounds) || !ch_sink_valid(services->draw))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    out_result->transform.struct_size = sizeof(out_result->transform);
    status = ch_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    memset(&transform, 0, sizeof(transform));
    transform.struct_size = sizeof(transform);
    status = fw_visual_transform_resolve(ch_intrinsic(request), bounds,
        &request->transform, &transform);
    if (status != FW_STATUS_OK) return status;
    out_result->transform = transform;
    out_result->uncovered_is_transparent =
        transform.uncovered_is_transparent;
    ch_cache_key(request, bounds, &out_result->cache_key_high,
        &out_result->cache_key_low);
    if (request->opacity == 0.0f || transform.destination.width == 0.0f ||
        transform.destination.height == 0.0f) return FW_STATUS_OK;
    memset(&emitter, 0, sizeof(emitter));
    emitter.sink = services->draw;
    emitter.first = emitter.sink->begin_chart(emitter.sink->user_data,
        &transform, request->opacity);
    if (emitter.first == FW_STATUS_OK) {
        emitter.began = 1u;
        ++emitter.commands;
    }
    plot = ch_make_plot(request);
    if (ch_can_emit(&emitter)) ch_emit_common(request, &plot, &emitter);
    if (ch_can_emit(&emitter)) {
        if (request->kind == FW_CHART_BAR ||
            request->kind == FW_CHART_HISTOGRAM)
            ch_emit_bar(request, &plot, &emitter);
        else if (request->kind == FW_CHART_LINE)
            ch_emit_line(request, &plot, &emitter);
        else if (request->kind == FW_CHART_PIE ||
            request->kind == FW_CHART_DONUT)
            ch_emit_pie(request, &emitter);
        else if (request->kind == FW_CHART_AREA)
            ch_emit_area(request, &plot, &emitter);
        else if (request->kind == FW_CHART_SCATTER ||
            request->kind == FW_CHART_BUBBLE)
            ch_emit_scatter(request, &plot, &emitter, 0);
        else if (request->kind == FW_CHART_RADAR)
            ch_emit_radar(request, &plot, &emitter);
        else if (request->kind == FW_CHART_HEATMAP)
            ch_emit_heatmap(request, &plot, &emitter);
        else if (request->kind == FW_CHART_GAUGE)
            ch_emit_gauge(request, &emitter);
        else if (request->kind == FW_CHART_BOX_PLOT)
            ch_emit_box(request, &plot, &emitter);
        else if (request->kind == FW_CHART_WATERFALL)
            ch_emit_waterfall(request, &plot, &emitter);
        else if (request->kind == FW_CHART_FUNNEL)
            ch_emit_funnel(request, &emitter);
        else if (request->kind == FW_CHART_CANDLESTICK)
            ch_emit_candlestick(request, &plot, &emitter);
        else if (request->kind == FW_CHART_TIME_SERIES)
            ch_emit_scatter(request, &plot, &emitter, 1);
        else if (request->kind == FW_CHART_COMBO) {
            ch_emit_bar(request, &plot, &emitter);
            if (ch_can_emit(&emitter)) ch_emit_area(request, &plot, &emitter);
            if (ch_can_emit(&emitter)) ch_emit_line(request, &plot, &emitter);
        }
        else if (request->kind == FW_CHART_DIVERGING_BAR)
            ch_emit_diverging_bar(request, &plot, &emitter);
        else if (request->kind == FW_CHART_FACET_LINE)
            ch_emit_facet_line(request, &plot, &emitter);
        else if (request->kind == FW_CHART_RANGE_AREA)
            ch_emit_range_area(request, &plot, &emitter);
        else if (request->kind == FW_CHART_DENSITY_HEATMAP)
            ch_emit_density_heatmap(request, &plot, &emitter);
        else if (request->kind == FW_CHART_WORD_CLOUD)
            ch_emit_word_cloud(request, &emitter);
        else if (request->kind == FW_CHART_ROSE)
            ch_emit_rose(request, &emitter);
    }
    if (emitter.began != 0u) {
        const fw_status ended = emitter.sink->end_chart(
            emitter.sink->user_data);
        if (ended == FW_STATUS_OK) ++emitter.commands;
        else if (emitter.first == FW_STATUS_OK) emitter.first = ended;
    }
    out_result->emitted_command_count = emitter.commands;
    out_result->rendered_series_count = ch_visible_series(request);
    out_result->rendered_value_count = ch_value_count(request);
    return emitter.first == FW_STATUS_OK ?
        FW_STATUS_OK : FW_STATUS_SINK_REJECTED;
}

static fw_status FW_CALL ch_element_render(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_element_override_v1 *overrides, size_t override_count,
    fw_rect_f32 bounds, const fw_chart_services_v1 *services,
    const fw_chart_element_observer_v1 *observer,
    fw_chart_render_result_v1 *out_result) {
    const char *key;
    fw_status status;
    ch_layer_sink_context context;
    fw_chart_draw_sink_v1 sink;
    fw_chart_services_v1 layered_services;
    if (services == NULL || services->struct_size < sizeof(*services) ||
        !ch_sink_valid(services->draw) ||
        (observer != NULL && (observer->struct_size < sizeof(*observer) ||
            observer->select == NULL))) return FW_STATUS_INVALID_ARGUMENT;
    status = ch_validate_element_overrides(plugin, request, overrides,
        override_count, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    memset(&context, 0, sizeof(context));
    context.request = request;
    context.downstream = services->draw;
    context.overrides = overrides;
    context.override_count = override_count;
    context.observer = observer;
    sink = ch_layer_sink(&context);
    layered_services = (fw_chart_services_v1){
        sizeof(layered_services), &sink, services->flags};
    status = ch_render(plugin, request, bounds, &layered_services,
        out_result);
    if (status == FW_STATUS_OK) ch_hash_element_overrides(
        &out_result->cache_key_high, &out_result->cache_key_low,
        overrides, override_count);
    return status;
}

typedef struct ch_theme_values {
    fw_color_rgba_f32 foreground;
    fw_color_rgba_f32 grid;
    fw_color_rgba_f32 palette[8];
    float line_width;
    float point_radius;
    float bar_gap;
    float fill_opacity;
} ch_theme_values;

typedef struct ch_label_box {
    float left;
    float top;
    float right;
    float bottom;
} ch_label_box;

typedef struct ch_presentation_sink_context {
    const fw_chart_renderer_request_v1 *request;
    const fw_chart_draw_sink_v1 *downstream;
    fw_chart_presentation_v1 presentation;
    fw_chart_presentation_plan_v1 plan;
    ch_label_box boxes[CH_MAX_WORDS];
    uint32_t box_count;
    uint32_t accepted_labels;
} ch_presentation_sink_context;

static fw_color_rgba_f32 ch_rgba(float red, float green, float blue,
    float alpha) {
    return (fw_color_rgba_f32){red, green, blue, alpha};
}

static ch_theme_values ch_theme(fw_chart_theme theme) {
    ch_theme_values result;
    memset(&result, 0, sizeof(result));
    result.foreground = ch_rgba(0.22f, 0.25f, 0.30f, 1.0f);
    result.grid = ch_rgba(0.76f, 0.79f, 0.83f, 0.28f);
    result.line_width = 0.004f;
    result.point_radius = 0.008f;
    result.bar_gap = 0.32f;
    result.fill_opacity = 0.22f;
    if (theme == FW_CHART_THEME_DARK) {
        result.foreground = ch_rgba(0.88f, 0.90f, 0.93f, 1.0f);
        result.grid = ch_rgba(0.48f, 0.52f, 0.59f, 0.24f);
        result.palette[0] = ch_rgba(0.45f, 0.63f, 0.82f, 1.0f);
        result.palette[1] = ch_rgba(0.48f, 0.66f, 0.63f, 1.0f);
        result.palette[2] = ch_rgba(0.95f, 0.61f, 0.36f, 1.0f);
        result.palette[3] = ch_rgba(0.61f, 0.55f, 0.72f, 1.0f);
        result.palette[4] = ch_rgba(0.72f, 0.48f, 0.51f, 1.0f);
        result.palette[5] = ch_rgba(0.47f, 0.62f, 0.72f, 1.0f);
        result.palette[6] = ch_rgba(0.58f, 0.65f, 0.49f, 1.0f);
        result.palette[7] = ch_rgba(0.70f, 0.60f, 0.49f, 1.0f);
    } else if (theme == FW_CHART_THEME_BUSINESS) {
        result.foreground = ch_rgba(0.20f, 0.25f, 0.32f, 1.0f);
        result.grid = ch_rgba(0.81f, 0.84f, 0.87f, 0.30f);
        result.palette[0] = ch_rgba(0.28f, 0.47f, 0.66f, 1.0f);
        result.palette[1] = ch_rgba(0.42f, 0.56f, 0.54f, 1.0f);
        result.palette[2] = ch_rgba(0.91f, 0.54f, 0.30f, 1.0f);
        result.palette[3] = ch_rgba(0.55f, 0.48f, 0.66f, 1.0f);
        result.palette[4] = ch_rgba(0.71f, 0.47f, 0.49f, 1.0f);
        result.palette[5] = ch_rgba(0.44f, 0.57f, 0.68f, 1.0f);
        result.palette[6] = ch_rgba(0.54f, 0.60f, 0.47f, 1.0f);
        result.palette[7] = ch_rgba(0.70f, 0.60f, 0.48f, 1.0f);
    } else if (theme == FW_CHART_THEME_ACADEMIC) {
        result.foreground = ch_rgba(0.24f, 0.24f, 0.25f, 1.0f);
        result.grid = ch_rgba(0.76f, 0.75f, 0.74f, 0.28f);
        result.palette[0] = ch_rgba(0.34f, 0.46f, 0.57f, 1.0f);
        result.palette[1] = ch_rgba(0.63f, 0.46f, 0.44f, 1.0f);
        result.palette[2] = ch_rgba(0.43f, 0.56f, 0.48f, 1.0f);
        result.palette[3] = ch_rgba(0.52f, 0.46f, 0.57f, 1.0f);
        result.palette[4] = ch_rgba(0.69f, 0.56f, 0.38f, 1.0f);
        result.palette[5] = ch_rgba(0.42f, 0.57f, 0.59f, 1.0f);
        result.palette[6] = ch_rgba(0.56f, 0.57f, 0.43f, 1.0f);
        result.palette[7] = ch_rgba(0.58f, 0.45f, 0.41f, 1.0f);
    } else if (theme == FW_CHART_THEME_HIGH_CONTRAST) {
        result.foreground = ch_rgba(0.0f, 0.0f, 0.0f, 1.0f);
        result.grid = ch_rgba(0.30f, 0.30f, 0.30f, 0.68f);
        result.line_width = 0.006f;
        result.point_radius = 0.012f;
        result.palette[0] = ch_rgba(0.0f, 0.24f, 0.85f, 1.0f);
        result.palette[1] = ch_rgba(0.85f, 0.12f, 0.05f, 1.0f);
        result.palette[2] = ch_rgba(0.0f, 0.52f, 0.20f, 1.0f);
        result.palette[3] = ch_rgba(0.58f, 0.0f, 0.72f, 1.0f);
        result.palette[4] = ch_rgba(0.90f, 0.55f, 0.0f, 1.0f);
        result.palette[5] = ch_rgba(0.0f, 0.60f, 0.70f, 1.0f);
        result.palette[6] = ch_rgba(0.45f, 0.30f, 0.0f, 1.0f);
        result.palette[7] = ch_rgba(0.75f, 0.0f, 0.34f, 1.0f);
    } else {
        result.palette[0] = ch_rgba(0.34f, 0.52f, 0.72f, 1.0f);
        result.palette[1] = ch_rgba(0.43f, 0.59f, 0.57f, 1.0f);
        result.palette[2] = ch_rgba(0.94f, 0.55f, 0.30f, 1.0f);
        result.palette[3] = ch_rgba(0.58f, 0.51f, 0.69f, 1.0f);
        result.palette[4] = ch_rgba(0.72f, 0.49f, 0.52f, 1.0f);
        result.palette[5] = ch_rgba(0.44f, 0.61f, 0.70f, 1.0f);
        result.palette[6] = ch_rgba(0.57f, 0.64f, 0.48f, 1.0f);
        result.palette[7] = ch_rgba(0.71f, 0.61f, 0.50f, 1.0f);
    }
    return result;
}

static fw_status ch_validate_presentation(
    const fw_chart_presentation_v1 *presentation) {
    if (presentation == NULL ||
        presentation->struct_size < sizeof(*presentation) ||
        presentation->theme > FW_CHART_THEME_HIGH_CONTRAST ||
        presentation->legend_placement > FW_CHART_LEGEND_HIDDEN ||
        presentation->label_policy > FW_CHART_LABEL_NONE ||
        !ch_bool(presentation->auto_layout) ||
        presentation->max_visible_labels > CH_MAX_WORDS ||
        !isfinite(presentation->label_padding) ||
        presentation->label_padding < 0.0f ||
        presentation->label_padding > 0.1f ||
        !isfinite(presentation->title_scale) ||
        presentation->title_scale <= 0.0f ||
        presentation->title_scale > 4.0f ||
        !isfinite(presentation->label_scale) ||
        presentation->label_scale <= 0.0f ||
        presentation->label_scale > 4.0f ||
        !isfinite(presentation->value_scale) ||
        presentation->value_scale <= 0.0f ||
        presentation->value_scale > 4.0f ||
        (presentation->flags & ~(FW_CHART_PRESENTATION_USE_THEME_PALETTE |
            FW_CHART_PRESENTATION_AVOID_COLLISIONS |
            FW_CHART_PRESENTATION_DIRECT_LABELS)) != 0u)
        return FW_STATUS_INVALID_ARGUMENT;
    return FW_STATUS_OK;
}

static fw_chart_presentation_plan_v1 ch_presentation_plan(
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_presentation_v1 *presentation) {
    fw_chart_presentation_plan_v1 result;
    uint32_t label_limit = presentation->max_visible_labels != 0u ?
        presentation->max_visible_labels : 64u;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.resolved_theme = presentation->theme;
    if (result.resolved_theme == FW_CHART_THEME_AUTO)
        result.resolved_theme = request->target.high_contrast != 0u ?
            FW_CHART_THEME_HIGH_CONTRAST :
            (request->target.prefers_dark != 0u ?
                FW_CHART_THEME_DARK : FW_CHART_THEME_LIGHT);
    result.resolved_legend_placement = presentation->legend_placement;
    if (result.resolved_legend_placement == FW_CHART_LEGEND_AUTO) {
        if (presentation->auto_layout == 0u)
            result.resolved_legend_placement = FW_CHART_LEGEND_BOTTOM;
        else
            result.resolved_legend_placement = request->series_count > 4u ?
                FW_CHART_LEGEND_RIGHT : FW_CHART_LEGEND_BOTTOM;
    }
    result.max_visible_labels = label_limit;
    if (presentation->auto_layout == 0u) {
        result.category_label_stride = 1u;
        result.value_label_stride = 1u;
    } else {
        result.category_label_stride = request->category_count > label_limit ?
            (uint32_t)((request->category_count + label_limit - 1u) /
                label_limit) : 1u;
        result.value_label_stride = ch_value_count(request) > label_limit ?
            (ch_value_count(request) + label_limit - 1u) / label_limit : 1u;
    }
    return result;
}

static int ch_label_boxes_intersect(const ch_label_box *a,
    const ch_label_box *b, float padding) {
    return a->left < b->right + padding &&
        a->right > b->left - padding &&
        a->top < b->bottom + padding &&
        a->bottom > b->top - padding;
}

static fw_status FW_CALL ch_present_begin(void *user_data,
    const fw_visual_transform_result_v1 *transform, float opacity) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    context->box_count = 0u;
    context->accepted_labels = 0u;
    return context->downstream->begin_chart(
        context->downstream->user_data, transform, opacity);
}

static fw_status FW_CALL ch_present_end(void *user_data) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    return context->downstream->end_chart(context->downstream->user_data);
}

static fw_status FW_CALL ch_present_rect(void *user_data, fw_rect_f32 rect,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    size_t series_index = 0u;
    const int is_legend_marker = ch_view_equal(category_id,
        ch_view(CH_LEGEND_MARKER_TAG)) &&
        ch_find_series(context->request, series_id, &series_index);
    if (is_legend_marker) {
        if (context->presentation.label_policy == FW_CHART_LABEL_NONE ||
            context->plan.resolved_legend_placement ==
                FW_CHART_LEGEND_HIDDEN ||
            (context->presentation.flags &
                FW_CHART_PRESENTATION_DIRECT_LABELS) != 0u)
            return FW_STATUS_OK;
        if (context->plan.resolved_legend_placement ==
            FW_CHART_LEGEND_RIGHT) {
            rect.x = 0.765f;
            rect.y = 0.18f + 0.055f * (float)series_index -
                rect.height * 0.5f;
        }
    }
    return context->downstream->fill_rect(context->downstream->user_data,
        rect, color, series_id, category_id);
}

static fw_status FW_CALL ch_present_line(void *user_data,
    fw_point_f32 start, fw_point_f32 end, float width,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    return context->downstream->stroke_line(context->downstream->user_data,
        start, end, width, color, series_id, category_id);
}

static fw_status FW_CALL ch_present_circle(void *user_data,
    fw_point_f32 center, float radius, fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    return context->downstream->fill_circle(context->downstream->user_data,
        center, radius, color, series_id, category_id);
}

static fw_status FW_CALL ch_present_sector(void *user_data,
    fw_point_f32 center, float outer_radius, float inner_radius,
    float start, float sweep, fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    return context->downstream->fill_sector(context->downstream->user_data,
        center, outer_radius, inner_radius, start, sweep, color,
        series_id, category_id);
}

static fw_status FW_CALL ch_present_polygon(void *user_data,
    const fw_point_f32 *points, size_t point_count,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    return context->downstream->fill_polygon(context->downstream->user_data,
        points, point_count, color, series_id, category_id);
}

static fw_status FW_CALL ch_present_label(void *user_data,
    fw_string_view text, fw_point_f32 anchor, float font_size,
    fw_color_rgba_f32 color, fw_string_view element_id) {
    ch_presentation_sink_context *context =
        (ch_presentation_sink_context *)user_data;
    size_t index;
    const int is_title = ch_view_equal(element_id,
        context->request->chart_id);
    const int is_series = ch_find_series(context->request, element_id,
        &index);
    const int is_legend = is_series && anchor.y >= 0.90f;
    const int is_category = !is_series && ch_find_category(
        context->request, element_id, &index);
    ch_label_box box;
    uint32_t i;
    if (!is_title && context->presentation.label_policy ==
        FW_CHART_LABEL_NONE) return FW_STATUS_OK;
    if (context->presentation.label_policy == FW_CHART_LABEL_IMPORTANT &&
        !is_title) {
        if (is_category && index != 0u &&
            index + 1u != context->request->category_count)
            return FW_STATUS_OK;
        if (is_series && !is_legend) return FW_STATUS_OK;
    }
    if (is_category && context->presentation.label_policy !=
        FW_CHART_LABEL_ALL && index %
        context->plan.category_label_stride != 0u) return FW_STATUS_OK;
    if (context->accepted_labels >= context->plan.max_visible_labels &&
        !is_title) return FW_STATUS_OK;
    if (is_title) font_size *= context->presentation.title_scale;
    else if (is_legend || is_category)
        font_size *= context->presentation.label_scale;
    else font_size *= context->presentation.value_scale;
    if (is_legend && (context->presentation.flags &
        FW_CHART_PRESENTATION_DIRECT_LABELS) != 0u) {
        anchor.x = 0.90f;
        anchor.y = 0.18f + 0.07f * (float)index;
    } else if (is_legend && context->plan.resolved_legend_placement ==
        FW_CHART_LEGEND_HIDDEN) return FW_STATUS_OK;
    else if (is_legend && context->plan.resolved_legend_placement ==
        FW_CHART_LEGEND_RIGHT) {
        anchor.x = 0.82f;
        anchor.y = 0.18f + 0.055f * (float)index;
    }
    box.left = anchor.x - fminf(0.42f,
        (float)ch_utf8_codepoints(text) * font_size * 0.29f);
    box.right = anchor.x + fminf(0.42f,
        (float)ch_utf8_codepoints(text) * font_size * 0.29f);
    box.top = anchor.y - font_size * 0.62f;
    box.bottom = anchor.y + font_size * 0.62f;
    if ((context->presentation.flags &
        FW_CHART_PRESENTATION_AVOID_COLLISIONS) != 0u && !is_title &&
        context->presentation.label_policy != FW_CHART_LABEL_ALL) {
        for (i = 0u; i < context->box_count; ++i)
            if (ch_label_boxes_intersect(&box, &context->boxes[i],
                context->presentation.label_padding))
                return FW_STATUS_OK;
    }
    if (context->box_count < CH_MAX_WORDS)
        context->boxes[context->box_count++] = box;
    if (!is_title) ++context->accepted_labels;
    return context->downstream->draw_label(context->downstream->user_data,
        text, anchor, font_size, color, element_id);
}

static fw_chart_draw_sink_v1 ch_presentation_sink(
    ch_presentation_sink_context *context) {
    return (fw_chart_draw_sink_v1){sizeof(fw_chart_draw_sink_v1), context,
        ch_present_begin, ch_present_end, ch_present_rect, ch_present_line,
        ch_present_circle, ch_present_sector, ch_present_polygon,
        ch_present_label};
}

static fw_status FW_CALL ch_presentation_validate(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_presentation_v1 *presentation,
    fw_chart_validation_result_v1 *out_result) {
    const char *key = "chart.presentation.invalid";
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    if (status == FW_STATUS_OK) {
        status = ch_validate_presentation(presentation);
        key = status == FW_STATUS_OK ? "chart.presentation.ok" :
            "chart.presentation.invalid";
    }
    out_result->status = status;
    out_result->diagnostic_key = ch_view(key);
    return status;
}

static fw_status FW_CALL ch_presentation_resolve(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_presentation_v1 *presentation,
    fw_chart_presentation_plan_v1 *out_plan) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_plan == NULL || out_plan->struct_size < sizeof(*out_plan))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_plan->struct_size;
    memset(out_plan, 0, sizeof(*out_plan));
    out_plan->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    if (status != FW_STATUS_OK) return status;
    status = ch_validate_presentation(presentation);
    if (status != FW_STATUS_OK) return status;
    *out_plan = ch_presentation_plan(request, presentation);
    out_plan->struct_size = size;
    return FW_STATUS_OK;
}

static fw_status FW_CALL ch_presentation_render(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request,
    const fw_chart_presentation_v1 *presentation,
    const fw_chart_element_override_v1 *overrides, size_t override_count,
    fw_rect_f32 bounds, const fw_chart_services_v1 *services,
    const fw_chart_element_observer_v1 *observer,
    fw_chart_render_result_v1 *out_result) {
    fw_chart_renderer_request_v1 themed_request;
    fw_chart_series_v1 *themed_series;
    ch_presentation_sink_context context;
    ch_layer_sink_context layer_context;
    fw_chart_draw_sink_v1 sink;
    fw_chart_draw_sink_v1 layer_sink;
    fw_chart_services_v1 themed_services;
    fw_chart_presentation_plan_v1 plan;
    ch_theme_values theme;
    const char *key;
    fw_status status;
    size_t i;
    if (services == NULL || services->struct_size < sizeof(*services) ||
        !ch_sink_valid(services->draw)) return FW_STATUS_INVALID_ARGUMENT;
    status = ch_validate_request(plugin, request, &key);
    if (status != FW_STATUS_OK) return status;
    status = ch_validate_presentation(presentation);
    if (status != FW_STATUS_OK) return status;
    status = ch_validate_element_overrides(plugin, request, overrides,
        override_count, &key);
    if (status != FW_STATUS_OK) return status;
    themed_series = (fw_chart_series_v1 *)calloc(request->series_count,
        sizeof(*themed_series));
    if (themed_series == NULL) return FW_STATUS_OUT_OF_MEMORY;
    plan = ch_presentation_plan(request, presentation);
    theme = ch_theme(plan.resolved_theme);
    themed_request = *request;
    themed_request.style = request->style;
    themed_request.style.foreground = theme.foreground;
    themed_request.style.grid_color = theme.grid;
    themed_request.style.line_width = theme.line_width;
    themed_request.style.point_radius = theme.point_radius;
    themed_request.style.bar_gap_ratio = theme.bar_gap;
    themed_request.style.fill_opacity = theme.fill_opacity;
    themed_request.style.flags |= CH_STYLE_INTERNAL_LEGEND_TAG;
    if (plan.resolved_legend_placement == FW_CHART_LEGEND_HIDDEN)
        themed_request.style.show_legend = 0u;
    for (i = 0u; i < request->series_count; ++i) {
        themed_series[i] = request->series[i];
        if ((presentation->flags &
            FW_CHART_PRESENTATION_USE_THEME_PALETTE) != 0u)
            themed_series[i].color = theme.palette[i % 8u];
    }
    themed_request.series = themed_series;
    memset(&layer_context, 0, sizeof(layer_context));
    layer_context.request = &themed_request;
    layer_context.downstream = services->draw;
    layer_context.overrides = overrides;
    layer_context.override_count = override_count;
    layer_context.observer = observer;
    layer_sink = ch_layer_sink(&layer_context);
    memset(&context, 0, sizeof(context));
    context.request = &themed_request;
    context.downstream = &layer_sink;
    context.presentation = *presentation;
    context.plan = plan;
    sink = ch_presentation_sink(&context);
    themed_services = (fw_chart_services_v1){sizeof(themed_services),
        &sink, services->flags};
    status = ch_render(plugin, &themed_request, bounds, &themed_services,
        out_result);
    if (status == FW_STATUS_OK) {
        ch_hash_element_overrides(&out_result->cache_key_high,
            &out_result->cache_key_low, overrides, override_count);
        ch_hash(&out_result->cache_key_high, &out_result->cache_key_low,
            presentation, sizeof(*presentation));
        ch_hash(&out_result->cache_key_high, &out_result->cache_key_low,
            &plan, sizeof(plan));
    }
    free(themed_series);
    return status;
}

static fw_status FW_CALL ch_build_semantics(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request, fw_rect_f32 bounds,
    fw_chart_semantics_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(*out_result) ||
        !ch_valid_rect(bounds)) return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    out_result->role = FW_SEMANTICS_ROLE_CHART;
    out_result->label = request->title.length == 0u ?
        request->chart_id : request->title;
    out_result->summary = request->summary;
    out_result->bounds = bounds;
    out_result->series_count = ch_visible_series(request);
    out_result->value_count = ch_value_count(request);
    return FW_STATUS_OK;
}

static int ch_point_to_normalized(fw_point_f32 point,
    const fw_visual_transform_result_v1 *transform,
    fw_point_f32 *out_point) {
    float x;
    float y;
    if (transform->clip_to_viewport != 0u &&
        (point.x < transform->viewport.x || point.y < transform->viewport.y ||
         point.x > transform->viewport.x + transform->viewport.width ||
         point.y > transform->viewport.y + transform->viewport.height))
        return 0;
    if (transform->destination.width <= 0.0f ||
        transform->destination.height <= 0.0f ||
        point.x < transform->destination.x ||
        point.y < transform->destination.y ||
        point.x > transform->destination.x + transform->destination.width ||
        point.y > transform->destination.y + transform->destination.height)
        return 0;
    x = (point.x - transform->destination.x) /
        transform->destination.width;
    y = (point.y - transform->destination.y) /
        transform->destination.height;
    switch (transform->content_rotation_quarter_turns) {
    case FW_VISUAL_ROTATION_90:
        out_point->x = y; out_point->y = 1.0f - x; break;
    case FW_VISUAL_ROTATION_180:
        out_point->x = 1.0f - x; out_point->y = 1.0f - y; break;
    case FW_VISUAL_ROTATION_270:
        out_point->x = 1.0f - y; out_point->y = x; break;
    default:
        out_point->x = x; out_point->y = y; break;
    }
    return out_point->x >= 0.0f && out_point->x <= 1.0f &&
        out_point->y >= 0.0f && out_point->y <= 1.0f;
}

static void ch_set_hit(fw_chart_hit_result_v1 *result,
    const fw_chart_renderer_request_v1 *request, uint32_t kind,
    size_t series, size_t category) {
    result->hit = 1u;
    result->element_kind = kind;
    result->series_index = (uint32_t)series;
    result->value_index = (uint32_t)category;
    result->series_id = request->series[series].id;
    result->category_id = request->categories[category].id;
    result->value = request->series[series].values[category].value;
}

static void ch_hit_bar(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, fw_point_f32 point,
    fw_chart_hit_result_v1 *result) {
    size_t category;
    size_t series;
    uint32_t visible_index;
    const float group_width = plot->width / (float)request->category_count;
    const float inner = group_width * (1.0f - request->style.bar_gap_ratio);
    const float bar_width = inner / (float)plot->visible_series;
    const float zero_y = ch_value_y(plot, 0.0);
    for (category = 0u; category < request->category_count; ++category) {
        visible_index = 0u;
        for (series = 0u; series < request->series_count; ++series) {
            const fw_chart_series_v1 *series_value = &request->series[series];
            const fw_chart_value_v1 *value;
            fw_rect_f32 rect;
            float value_y;
            if (series_value->visible == 0u) continue;
            value = &series_value->values[category];
            if (value->missing != 0u) { ++visible_index; continue; }
            value_y = ch_value_y(plot, value->value);
            rect.x = plot->x + group_width * (float)category +
                (group_width - inner) * 0.5f +
                bar_width * (float)visible_index;
            rect.y = value_y < zero_y ? value_y : zero_y;
            rect.width = bar_width;
            rect.height = fabsf(value_y - zero_y);
            if (point.x >= rect.x && point.x <= rect.x + rect.width &&
                point.y >= rect.y && point.y <= rect.y + rect.height) {
                ch_set_hit(result, request, FW_CHART_ELEMENT_BAR,
                    series, category);
                return;
            }
            ++visible_index;
        }
    }
}

static void ch_hit_line(const fw_chart_renderer_request_v1 *request,
    const ch_plot *plot, fw_point_f32 point,
    fw_chart_hit_result_v1 *result) {
    size_t series;
    size_t category;
    float best = 1.0e30f;
    const float radius = request->style.point_radius > 0.025f ?
        request->style.point_radius : 0.025f;
    for (series = 0u; series < request->series_count; ++series) {
        if (request->series[series].visible == 0u) continue;
        for (category = 0u; category < request->category_count; ++category) {
            const fw_chart_value_v1 *value =
                &request->series[series].values[category];
            float x;
            float y;
            float dx;
            float dy;
            float distance;
            if (value->missing != 0u) continue;
            x = plot->x + plot->width * ((float)category + 0.5f) /
                (float)request->category_count;
            y = ch_value_y(plot, value->value);
            dx = point.x - x;
            dy = point.y - y;
            distance = dx * dx + dy * dy;
            if (distance <= radius * radius && distance < best) {
                best = distance;
                ch_set_hit(result, request, FW_CHART_ELEMENT_LINE_POINT,
                    series, category);
            }
        }
    }
}

static void ch_hit_pie(const fw_chart_renderer_request_v1 *request,
    fw_point_f32 point, fw_chart_hit_result_v1 *result) {
    size_t category;
    const fw_chart_series_v1 *series = &request->series[0];
    const double dx = point.x - 0.5;
    const double dy = point.y - 0.46;
    const double distance_squared = dx * dx + dy * dy;
    const double inner = request->kind == FW_CHART_DONUT ?
        (request->style.donut_inner_radius > 0.0f ?
            0.34 * request->style.donut_inner_radius : 0.18) : 0.0;
    double angle;
    double cursor = 0.0;
    double total = 0.0;
    if (distance_squared > 0.34 * 0.34 ||
        distance_squared < inner * inner) return;
    angle = atan2(dy, dx) + CH_PI * 0.5;
    while (angle < 0.0) angle += CH_PI * 2.0;
    while (angle >= CH_PI * 2.0) angle -= CH_PI * 2.0;
    for (category = 0u; category < request->category_count; ++category)
        if (series->values[category].missing == 0u)
            total += series->values[category].value;
    for (category = 0u; category < request->category_count; ++category) {
        const fw_chart_value_v1 *value = &series->values[category];
        double sweep;
        if (value->missing != 0u || value->value <= 0.0) continue;
        sweep = value->value / total * CH_PI * 2.0;
        if (angle >= cursor && angle <= cursor + sweep) {
            ch_set_hit(result, request, FW_CHART_ELEMENT_PIE_SLICE,
                0u, category);
            return;
        }
        cursor += sweep;
    }
}

static fw_status FW_CALL ch_hit_test(fw_plugin_handle plugin,
    const fw_chart_renderer_request_v1 *request, fw_rect_f32 bounds,
    fw_point_f32 point, fw_chart_hit_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    fw_visual_transform_result_v1 transform;
    fw_point_f32 normalized;
    ch_plot plot;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(*out_result) ||
        !ch_valid_rect(bounds) || !isfinite(point.x) || !isfinite(point.y))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = ch_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    memset(&transform, 0, sizeof(transform));
    transform.struct_size = sizeof(transform);
    status = fw_visual_transform_resolve(ch_intrinsic(request), bounds,
        &request->transform, &transform);
    if (status != FW_STATUS_OK) return status;
    if (!ch_point_to_normalized(point, &transform, &normalized))
        return FW_STATUS_OK;
    out_result->normalized_point = normalized;
    plot = ch_make_plot(request);
    if (request->kind == FW_CHART_BAR ||
        request->kind == FW_CHART_HISTOGRAM)
        ch_hit_bar(request, &plot, normalized, out_result);
    else if (request->kind == FW_CHART_LINE)
        ch_hit_line(request, &plot, normalized, out_result);
    else if (request->kind == FW_CHART_PIE ||
        request->kind == FW_CHART_DONUT)
        ch_hit_pie(request, normalized, out_result);
    return FW_STATUS_OK;
}

static fw_status FW_CALL ch_get_parameter_schema(fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!ch_context_valid(plugin) || out_schema_json == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    out_schema_json->data = ch_parameter_schema;
    out_schema_json->length = sizeof(ch_parameter_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_chart_renderer_api_v1 ch_renderer_api = {
    sizeof(fw_chart_renderer_api_v1), FW_CHART_RENDERER_INTERFACE_VERSION,
    ch_validate, ch_measure, ch_render, ch_build_semantics, ch_hit_test,
    ch_get_parameter_schema};

static const fw_chart_element_api_v1 ch_element_api = {
    sizeof(fw_chart_element_api_v1), FW_CHART_ELEMENT_INTERFACE_VERSION,
    ch_element_validate_overrides, ch_element_enumerate,
    ch_element_format_id, ch_element_render};

static const fw_chart_presentation_api_v1 ch_presentation_api = {
    sizeof(fw_chart_presentation_api_v1),
    FW_CHART_PRESENTATION_INTERFACE_VERSION,
    ch_presentation_validate, ch_presentation_resolve,
    ch_presentation_render};

static const fw_plugin_descriptor_v1 *FW_CALL ch_get_descriptor(void) {
    return &ch_descriptor;
}

static fw_status FW_CALL ch_load(const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    ch_context *context;
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL || host->struct_size < sizeof(*host) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR)
        return FW_STATUS_INVALID_ARGUMENT;
    context = (ch_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FW_STATUS_OUT_OF_MEMORY;
    context->magic = CH_MAGIC;
    context->host = *host;
    *out_handle = context;
    return FW_STATUS_OK;
}

static void FW_CALL ch_unload(fw_plugin_handle handle) {
    ch_context *context = (ch_context *)handle;
    if (context != NULL && context->magic == CH_MAGIC) {
        context->magic = 0u;
        free(context);
    }
}

static fw_status FW_CALL ch_query_interface(fw_plugin_handle handle,
    fw_string_view id, uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (!ch_context_valid(handle) || !ch_string_shape(id))
        return FW_STATUS_INVALID_ARGUMENT;
    if (ch_string_equal(id, FW_CHART_RENDERER_INTERFACE_ID)) {
        if (minimum_version > FW_CHART_RENDERER_INTERFACE_VERSION)
            return FW_STATUS_NOT_FOUND;
        *out_interface = &ch_renderer_api;
        return FW_STATUS_OK;
    }
    if (ch_string_equal(id, FW_CHART_ELEMENT_INTERFACE_ID)) {
        if (minimum_version > FW_CHART_ELEMENT_INTERFACE_VERSION)
            return FW_STATUS_NOT_FOUND;
        *out_interface = &ch_element_api;
        return FW_STATUS_OK;
    }
    if (ch_string_equal(id, FW_CHART_PRESENTATION_INTERFACE_ID)) {
        if (minimum_version > FW_CHART_PRESENTATION_INTERFACE_VERSION)
            return FW_STATUS_NOT_FOUND;
        *out_interface = &ch_presentation_api;
        return FW_STATUS_OK;
    }
    return FW_STATUS_NOT_FOUND;
}

static const fw_plugin_api_v1 ch_plugin_api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    ch_get_descriptor, ch_load, ch_unload, ch_query_interface};

#if defined(FACETWIRE_CORE_CHART_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_core_chart_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) return NULL;
    return &ch_plugin_api;
}

#if defined(FACETWIRE_CORE_CHART_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    return facetwire_core_chart_plugin_query(requested_abi);
}
#endif
