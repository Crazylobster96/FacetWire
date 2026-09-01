/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_chart_demo.h"

#include <facetwire/chart_element_layer.h>
#include <facetwire/chart_presentation.h>
#include <facetwire/chart_renderer.h>
#include <facetwire/hierarchical_chart_renderer.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FWCHART_MAX_COMMANDS 512u
#define FWCHART_MAX_ELEMENTS 128u
#define FWCHART_JSON_CAPACITY 262144u

typedef enum fwchart_command_kind {
    FWCHART_COMMAND_RECT = 1,
    FWCHART_COMMAND_LINE = 2,
    FWCHART_COMMAND_CIRCLE = 3,
    FWCHART_COMMAND_SECTOR = 4,
    FWCHART_COMMAND_LABEL = 5,
    FWCHART_COMMAND_POLYGON = 6
} fwchart_command_kind;

typedef struct fwchart_command {
    uint32_t kind;
    float values[10];
    uint32_t value_count;
    fw_color_rgba_f32 color;
    char series_id[32];
    char category_id[32];
    char text[64];
    char element_id[160];
    int32_t z_index;
    uint32_t promoted;
} fwchart_command;

typedef struct fwchart_sink_state {
    fw_visual_transform_result_v1 transform;
    float opacity;
    uint32_t begin_count;
    uint32_t end_count;
    uint32_t command_count;
    fwchart_command commands[FWCHART_MAX_COMMANDS];
    const fw_chart_element_api_v1 *elements;
    fw_plugin_handle plugin;
    char current_element_id[160];
    int32_t current_z_index;
    uint32_t current_promoted;
} fwchart_sink_state;

typedef struct fwchart_element_record {
    fw_chart_element_ref_v1 ref;
    fw_rect_f32 bounds;
    uint32_t role;
    int32_t z_index;
    uint32_t capabilities;
    uint32_t flags;
    char id[160];
    char label[64];
} fwchart_element_record;

typedef struct fwchart_element_state {
    const fw_chart_element_api_v1 *api;
    fw_plugin_handle plugin;
    uint32_t count;
    fwchart_element_record records[FWCHART_MAX_ELEMENTS];
} fwchart_element_state;

struct fwchart_context {
    const fw_plugin_api_v1 *plugin;
    fw_plugin_handle handle;
    const fw_chart_renderer_api_v1 *renderer;
    const fw_chart_element_api_v1 *elements;
    const fw_chart_presentation_api_v1 *presentation;
    const fw_plugin_api_v1 *hierarchy_plugin;
    fw_plugin_handle hierarchy_handle;
    const fw_hierarchical_chart_api_v1 *hierarchy;
};

static int chart_buffer_available(const fwchart_buffer *buffer) {
    return buffer != NULL && buffer->data == NULL && buffer->length == 0u;
}

static fw_string_view chart_view(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static void chart_copy_view(char *target, size_t capacity,
    fw_string_view source) {
    size_t length = source.length;
    if (capacity == 0u) return;
    if (length >= capacity) length = capacity - 1u;
    if (length != 0u) memcpy(target, source.data, length);
    target[length] = '\0';
}

static fwchart_status chart_copy_text(const char *text, size_t length,
    fwchart_buffer *out_buffer) {
    uint8_t *copy;
    if (text == NULL || !chart_buffer_available(out_buffer))
        return FWCHART_STATUS_INVALID_ARGUMENT;
    copy = (uint8_t *)calloc(length + 1u, 1u);
    if (copy == NULL) return FWCHART_STATUS_OUT_OF_MEMORY;
    if (length != 0u) memcpy(copy, text, length);
    out_buffer->data = copy;
    out_buffer->length = (uint64_t)length;
    return FWCHART_STATUS_OK;
}

static int chart_append(char *buffer, size_t capacity, size_t *offset,
    const char *format, ...) {
    int written;
    va_list arguments;
    if (*offset >= capacity) return 0;
    va_start(arguments, format);
    written = vsnprintf(buffer + *offset, capacity - *offset,
        format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *offset) return 0;
    *offset += (size_t)written;
    return 1;
}

static fwchart_command *chart_command(fwchart_sink_state *state,
    uint32_t kind, fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fwchart_command *command;
    if (state == NULL || state->command_count >= FWCHART_MAX_COMMANDS)
        return NULL;
    command = &state->commands[state->command_count++];
    memset(command, 0, sizeof(*command));
    command->kind = kind;
    command->value_count = 5u;
    command->color = color;
    chart_copy_view(command->series_id, sizeof(command->series_id), series_id);
    chart_copy_view(command->category_id, sizeof(command->category_id),
        category_id);
    chart_copy_view(command->element_id, sizeof(command->element_id),
        chart_view(state->current_element_id));
    command->z_index = state->current_z_index;
    command->promoted = state->current_promoted;
    return command;
}

static fw_status FW_CALL chart_begin(void *user_data,
    const fw_visual_transform_result_v1 *transform, float opacity) {
    fwchart_sink_state *state = (fwchart_sink_state *)user_data;
    if (state == NULL || transform == NULL ||
        transform->struct_size < sizeof(*transform))
        return FW_STATUS_INVALID_ARGUMENT;
    state->transform = *transform;
    state->opacity = opacity;
    ++state->begin_count;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_end(void *user_data) {
    fwchart_sink_state *state = (fwchart_sink_state *)user_data;
    if (state == NULL || state->end_count >= state->begin_count)
        return FW_STATUS_INVALID_STATE;
    ++state->end_count;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_rect(void *user_data, fw_rect_f32 rect,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fwchart_command *command = chart_command((fwchart_sink_state *)user_data,
        FWCHART_COMMAND_RECT, color, series_id, category_id);
    if (command == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    command->values[0] = rect.x;
    command->values[1] = rect.y;
    command->values[2] = rect.width;
    command->values[3] = rect.height;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_line(void *user_data, fw_point_f32 start,
    fw_point_f32 end, float width, fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    fwchart_command *command = chart_command((fwchart_sink_state *)user_data,
        FWCHART_COMMAND_LINE, color, series_id, category_id);
    if (command == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    command->values[0] = start.x;
    command->values[1] = start.y;
    command->values[2] = end.x;
    command->values[3] = end.y;
    command->values[4] = width;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_circle(void *user_data, fw_point_f32 center,
    float radius, fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fwchart_command *command = chart_command((fwchart_sink_state *)user_data,
        FWCHART_COMMAND_CIRCLE, color, series_id, category_id);
    if (command == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    command->values[0] = center.x;
    command->values[1] = center.y;
    command->values[2] = radius;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_sector(void *user_data, fw_point_f32 center,
    float outer_radius, float inner_radius, float start, float sweep,
    fw_color_rgba_f32 color,
    fw_string_view series_id, fw_string_view category_id) {
    fwchart_command *command = chart_command((fwchart_sink_state *)user_data,
        FWCHART_COMMAND_SECTOR, color, series_id, category_id);
    if (command == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    command->values[0] = center.x;
    command->values[1] = center.y;
    command->values[2] = outer_radius;
    command->values[3] = inner_radius;
    command->values[4] = start;
    command->values[5] = sweep;
    command->value_count = 6u;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_polygon(void *user_data,
    const fw_point_f32 *points, size_t point_count,
    fw_color_rgba_f32 color, fw_string_view series_id,
    fw_string_view category_id) {
    fwchart_command *command;
    size_t i;
    if (points == NULL || point_count < 3u || point_count > 5u)
        return FW_STATUS_CAPACITY_EXCEEDED;
    command = chart_command((fwchart_sink_state *)user_data,
        FWCHART_COMMAND_POLYGON, color, series_id, category_id);
    if (command == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    command->value_count = (uint32_t)(point_count * 2u);
    for (i = 0u; i < point_count; ++i) {
        command->values[i * 2u] = points[i].x;
        command->values[i * 2u + 1u] = points[i].y;
    }
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_label(void *user_data, fw_string_view text,
    fw_point_f32 anchor, float font_size, fw_color_rgba_f32 color,
    fw_string_view element_id) {
    const fw_string_view empty = {NULL, 0u};
    fwchart_command *command = chart_command((fwchart_sink_state *)user_data,
        FWCHART_COMMAND_LABEL, color, element_id, empty);
    if (command == NULL) return FW_STATUS_CAPACITY_EXCEEDED;
    command->values[0] = anchor.x;
    command->values[1] = anchor.y;
    command->values[2] = font_size;
    chart_copy_view(command->text, sizeof(command->text), text);
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_element_visit(void *user_data,
    const fw_chart_element_descriptor_v1 *descriptor) {
    fwchart_element_state *state = (fwchart_element_state *)user_data;
    fwchart_element_record *record;
    size_t required = 0u;
    if (state == NULL || descriptor == NULL ||
        descriptor->struct_size < sizeof(*descriptor) ||
        state->count >= FWCHART_MAX_ELEMENTS) return FW_STATUS_RESOURCE_LIMIT;
    record = &state->records[state->count];
    memset(record, 0, sizeof(*record));
    record->ref = descriptor->ref;
    record->bounds = descriptor->normalized_bounds;
    record->role = descriptor->ref.role;
    record->z_index = descriptor->z_index;
    record->capabilities = descriptor->capabilities;
    record->flags = descriptor->flags;
    chart_copy_view(record->label, sizeof(record->label), descriptor->label);
    if (state->api->format_element_id(state->plugin, &descriptor->ref,
        record->id, sizeof(record->id), &required) != FW_STATUS_OK)
        return FW_STATUS_BUFFER_TOO_SMALL;
    ++state->count;
    return FW_STATUS_OK;
}

static fw_status FW_CALL chart_element_observe(void *user_data,
    const fw_chart_element_descriptor_v1 *descriptor,
    const fw_chart_element_presentation_v1 *presentation) {
    fwchart_sink_state *state = (fwchart_sink_state *)user_data;
    size_t required = 0u;
    if (state == NULL || descriptor == NULL || presentation == NULL ||
        descriptor->struct_size < sizeof(*descriptor) ||
        presentation->struct_size < sizeof(*presentation))
        return FW_STATUS_INVALID_ARGUMENT;
    if (state->elements->format_element_id(state->plugin, &descriptor->ref,
        state->current_element_id, sizeof(state->current_element_id),
        &required) != FW_STATUS_OK) return FW_STATUS_BUFFER_TOO_SMALL;
    state->current_z_index = descriptor->z_index + presentation->z_offset;
    state->current_promoted = presentation->promotion ==
        FW_CHART_ELEMENT_PROMOTED ? 1u : 0u;
    return FW_STATUS_OK;
}

static const char *chart_kind_name(uint32_t kind) {
    static const char *names[] = {"bar", "line", "pie",
        "horizontal-bar", "stacked-bar", "percent-bar", "area",
        "stacked-area", "scatter", "bubble", "donut", "radar",
        "heatmap", "gauge", "box-plot", "histogram", "waterfall",
        "funnel", "candlestick", "time-series", "combo",
        "diverging-bar", "facet-line", "range-area", "density-heatmap",
        "word-cloud", "rose", "treemap", "sunburst", "packed-bubble"};
    return kind < sizeof(names) / sizeof(names[0]) ? names[kind] : "bar";
}

static const char *chart_command_name(uint32_t kind) {
    if (kind == FWCHART_COMMAND_LINE) return "line";
    if (kind == FWCHART_COMMAND_CIRCLE) return "circle";
    if (kind == FWCHART_COMMAND_SECTOR) return "sector";
    if (kind == FWCHART_COMMAND_LABEL) return "label";
    if (kind == FWCHART_COMMAND_POLYGON) return "polygon";
    return "rect";
}

static fwchart_status chart_serialize(const char *plugin_id,
    const char *capability, uint32_t kind,
    const fw_chart_render_result_v1 *render,
    const fw_chart_semantics_v1 *semantics,
    const fwchart_sink_state *state,
    const fwchart_element_state *element_state,
    uint32_t selected_element_index,
    fwchart_buffer *out_buffer) {
    char json[FWCHART_JSON_CAPACITY];
    size_t offset = 0u;
    uint32_t i;
    if (!chart_append(json, sizeof(json), &offset,
        "{\"pluginId\":\"%s\",\"capability\":\"%s\","
        "\"interfaceVersion\":1,\"nativeRuntime\":true,"
        "\"kind\":\"%s\",\"opacity\":%.4f,"
        "\"commandsBalanced\":%s,\"commandCount\":%u,"
        "\"renderedSeries\":%u,\"renderedValues\":%u,"
        "\"semanticRole\":%u,\"semanticValues\":%u,"
        "\"uncoveredIsTransparent\":%s,"
        "\"transform\":{\"rotation\":%u,"
        "\"clip\":%s,\"destination\":{\"x\":%.4f,\"y\":%.4f,"
        "\"width\":%.4f,\"height\":%.4f}},"
        "\"selectedElementIndex\":%u,\"elements\":[",
        plugin_id, capability, chart_kind_name(kind), state->opacity,
        state->begin_count == state->end_count ? "true" : "false",
        state->command_count, render->rendered_series_count,
        render->rendered_value_count, semantics->role,
        semantics->value_count,
        render->uncovered_is_transparent != 0u ? "true" : "false",
        render->transform.content_rotation_quarter_turns,
        render->transform.clip_to_viewport != 0u ? "true" : "false",
        render->transform.destination.x, render->transform.destination.y,
        render->transform.destination.width,
        render->transform.destination.height, selected_element_index))
        return FWCHART_STATUS_OUT_OF_MEMORY;
    for (i = 0u; i < element_state->count; ++i) {
        const fwchart_element_record *element = &element_state->records[i];
        if (!chart_append(json, sizeof(json), &offset,
            "%s{\"index\":%u,\"id\":\"%s\",\"role\":%u,"
            "\"label\":\"%s\",\"bounds\":[%.6f,%.6f,%.6f,%.6f],"
            "\"zIndex\":%d,\"capabilities\":%u,\"flags\":%u}",
            i == 0u ? "" : ",", i, element->id, element->role,
            element->label, element->bounds.x, element->bounds.y,
            element->bounds.width, element->bounds.height,
            element->z_index, element->capabilities, element->flags))
            return FWCHART_STATUS_OUT_OF_MEMORY;
    }
    if (!chart_append(json, sizeof(json), &offset, "],\"commands\":["))
        return FWCHART_STATUS_OUT_OF_MEMORY;
    for (i = 0u; i < state->command_count; ++i) {
        const fwchart_command *command = &state->commands[i];
        uint32_t value_index;
        if (!chart_append(json, sizeof(json), &offset,
            "%s{\"type\":\"%s\",\"v\":[",
            i == 0u ? "" : ",", chart_command_name(command->kind)))
            return FWCHART_STATUS_OUT_OF_MEMORY;
        for (value_index = 0u; value_index < command->value_count;
            ++value_index) {
            if (!chart_append(json, sizeof(json), &offset, "%s%.6f",
                value_index == 0u ? "" : ",",
                command->values[value_index]))
                return FWCHART_STATUS_OUT_OF_MEMORY;
        }
        if (!chart_append(json, sizeof(json), &offset,
            "],"
            "\"color\":[%.4f,%.4f,%.4f,%.4f],"
            "\"seriesId\":\"%s\",\"categoryId\":\"%s\","
            "\"text\":\"%s\",\"elementId\":\"%s\","
            "\"zIndex\":%d,\"promoted\":%s}",
            command->color.red, command->color.green,
            command->color.blue, command->color.alpha,
            command->series_id, command->category_id, command->text,
            command->element_id, command->z_index,
            command->promoted != 0u ? "true" : "false"))
            return FWCHART_STATUS_OUT_OF_MEMORY;
    }
    if (!chart_append(json, sizeof(json), &offset, "]}"))
        return FWCHART_STATUS_OUT_OF_MEMORY;
    return chart_copy_text(json, offset, out_buffer);
}

fwchart_status fwchart_context_create(fwchart_context **out_context) {
    fwchart_context *context;
    fw_host_api_v1 host;
    const void *interface_value = NULL;
    fw_status status;
    if (out_context == NULL) return FWCHART_STATUS_INVALID_ARGUMENT;
    *out_context = NULL;
    context = (fwchart_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FWCHART_STATUS_OUT_OF_MEMORY;
    context->plugin = facetwire_core_chart_plugin_query(
        FW_ABI_VERSION_CURRENT);
    if (context->plugin == NULL) { free(context); return FWCHART_STATUS_PLUGIN_ERROR; }
    memset(&host, 0, sizeof(host));
    host.struct_size = sizeof(host);
    host.abi_version = FW_ABI_VERSION_CURRENT;
    status = context->plugin->load(&host, &context->handle);
    if (status == FW_STATUS_OK)
        status = context->plugin->query_interface(context->handle,
            chart_view(FW_CHART_RENDERER_INTERFACE_ID),
            FW_CHART_RENDERER_INTERFACE_VERSION, &interface_value);
    if (status != FW_STATUS_OK || interface_value == NULL) {
        if (context->handle != NULL) context->plugin->unload(context->handle);
        free(context);
        return FWCHART_STATUS_PLUGIN_ERROR;
    }
    context->renderer = (const fw_chart_renderer_api_v1 *)interface_value;
    interface_value = NULL;
    status = context->plugin->query_interface(context->handle,
        chart_view(FW_CHART_ELEMENT_INTERFACE_ID),
        FW_CHART_ELEMENT_INTERFACE_VERSION, &interface_value);
    if (status != FW_STATUS_OK || interface_value == NULL) {
        context->plugin->unload(context->handle);
        free(context);
        return FWCHART_STATUS_PLUGIN_ERROR;
    }
    context->elements = (const fw_chart_element_api_v1 *)interface_value;
    interface_value = NULL;
    status = context->plugin->query_interface(context->handle,
        chart_view(FW_CHART_PRESENTATION_INTERFACE_ID),
        FW_CHART_PRESENTATION_INTERFACE_VERSION, &interface_value);
    if (status != FW_STATUS_OK || interface_value == NULL) {
        context->plugin->unload(context->handle);
        free(context);
        return FWCHART_STATUS_PLUGIN_ERROR;
    }
    context->presentation =
        (const fw_chart_presentation_api_v1 *)interface_value;
    context->hierarchy_plugin = facetwire_hierarchical_chart_plugin_query(
        FW_ABI_VERSION_CURRENT);
    if (context->hierarchy_plugin == NULL) {
        context->plugin->unload(context->handle);
        free(context);
        return FWCHART_STATUS_PLUGIN_ERROR;
    }
    interface_value = NULL;
    status = context->hierarchy_plugin->load(&host,
        &context->hierarchy_handle);
    if (status == FW_STATUS_OK)
        status = context->hierarchy_plugin->query_interface(
            context->hierarchy_handle,
            chart_view(FW_HIERARCHICAL_CHART_INTERFACE_ID),
            FW_HIERARCHICAL_CHART_INTERFACE_VERSION, &interface_value);
    if (status != FW_STATUS_OK || interface_value == NULL) {
        if (context->hierarchy_handle != NULL)
            context->hierarchy_plugin->unload(context->hierarchy_handle);
        context->plugin->unload(context->handle);
        free(context);
        return FWCHART_STATUS_PLUGIN_ERROR;
    }
    context->hierarchy =
        (const fw_hierarchical_chart_api_v1 *)interface_value;
    *out_context = context;
    return FWCHART_STATUS_OK;
}

void fwchart_context_destroy(fwchart_context *context) {
    if (context != NULL) {
        if (context->hierarchy_plugin != NULL &&
            context->hierarchy_handle != NULL)
            context->hierarchy_plugin->unload(context->hierarchy_handle);
        if (context->plugin != NULL && context->handle != NULL)
            context->plugin->unload(context->handle);
        context->renderer = NULL;
        context->elements = NULL;
        context->presentation = NULL;
        context->hierarchy = NULL;
        context->hierarchy_handle = NULL;
        context->handle = NULL;
        free(context);
    }
}

static fw_color_rgba_f32 chart_hierarchy_color(uint32_t theme,
    uint32_t index) {
    static const fw_color_rgba_f32 light[] = {
        {0.34f, 0.52f, 0.72f, 0.92f}, {0.43f, 0.59f, 0.57f, 0.92f},
        {0.94f, 0.55f, 0.30f, 0.92f}, {0.58f, 0.51f, 0.69f, 0.92f},
        {0.72f, 0.49f, 0.52f, 0.92f}, {0.44f, 0.61f, 0.70f, 0.92f},
        {0.57f, 0.64f, 0.48f, 0.92f}};
    static const fw_color_rgba_f32 dark[] = {
        {0.45f, 0.63f, 0.82f, 0.92f}, {0.48f, 0.66f, 0.63f, 0.92f},
        {0.95f, 0.61f, 0.36f, 0.92f}, {0.61f, 0.55f, 0.72f, 0.92f},
        {0.72f, 0.48f, 0.51f, 0.92f}, {0.47f, 0.62f, 0.72f, 0.92f},
        {0.58f, 0.65f, 0.49f, 0.92f}};
    static const fw_color_rgba_f32 business[] = {
        {0.28f, 0.47f, 0.66f, 0.92f}, {0.42f, 0.56f, 0.54f, 0.92f},
        {0.91f, 0.54f, 0.30f, 0.92f}, {0.55f, 0.48f, 0.66f, 0.92f},
        {0.71f, 0.47f, 0.49f, 0.92f}, {0.44f, 0.57f, 0.68f, 0.92f},
        {0.54f, 0.60f, 0.47f, 0.92f}};
    static const fw_color_rgba_f32 academic[] = {
        {0.20f, 0.35f, 0.52f, 0.90f}, {0.31f, 0.52f, 0.48f, 0.90f},
        {0.70f, 0.50f, 0.28f, 0.90f}, {0.47f, 0.38f, 0.57f, 0.90f},
        {0.63f, 0.37f, 0.38f, 0.90f}, {0.35f, 0.55f, 0.62f, 0.90f},
        {0.48f, 0.57f, 0.36f, 0.90f}};
    static const fw_color_rgba_f32 contrast[] = {
        {0.00f, 0.32f, 0.74f, 1.0f}, {0.00f, 0.58f, 0.48f, 1.0f},
        {0.92f, 0.45f, 0.00f, 1.0f}, {0.48f, 0.22f, 0.72f, 1.0f},
        {0.82f, 0.05f, 0.20f, 1.0f}, {0.00f, 0.54f, 0.78f, 1.0f},
        {0.43f, 0.57f, 0.00f, 1.0f}};
    if (theme == FW_CHART_THEME_DARK) return dark[index % 7u];
    if (theme == FW_CHART_THEME_BUSINESS) return business[index % 7u];
    if (theme == FW_CHART_THEME_ACADEMIC) return academic[index % 7u];
    if (theme == FW_CHART_THEME_HIGH_CONTRAST)
        return contrast[index % 7u];
    return light[index % 7u];
}

static fwchart_status chart_render_hierarchy(fwchart_context *context,
    float width, float height, uint32_t kind, uint32_t rotation,
    float opacity, uint32_t theme, uint32_t labels,
    fwchart_buffer *out_report_utf8_json) {
    static const char *ids[] = {"root", "platform", "desktop", "mobile",
        "services", "render", "layout"};
    static const char *names[] = {"FacetWire", "Platforms", "Desktop",
        "Mobile", "Services", "Render", "Layout"};
    static const uint32_t parents[] = {FW_HIERARCHICAL_ROOT_INDEX, 0u, 1u,
        1u, 0u, 4u, 4u};
    static const double values[] = {100.0, 58.0, 34.0, 24.0, 42.0, 25.0, 17.0};
    fw_hierarchical_chart_node_v1 nodes[7];
    fw_hierarchical_chart_request_v1 request;
    fwchart_sink_state state;
    fw_chart_draw_sink_v1 sink;
    fw_chart_services_v1 services;
    fw_chart_render_result_v1 render;
    fw_chart_semantics_v1 semantics;
    fwchart_element_state elements;
    fw_status status;
    size_t index;
    memset(nodes, 0, sizeof(nodes));
    for (index = 0u; index < 7u; ++index) {
        nodes[index].struct_size = sizeof(nodes[index]);
        nodes[index].id = chart_view(ids[index]);
        nodes[index].label = chart_view(names[index]);
        nodes[index].parent_index = parents[index];
        nodes[index].value = values[index];
        nodes[index].color = chart_hierarchy_color(theme, (uint32_t)index);
        nodes[index].visible = 1u;
    }
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.request_id = 2u;
    request.zone_id = chart_view("zone:hierarchy-demo");
    request.chart_id = chart_view("chart:hierarchy");
    request.title = chart_view("FacetWire Capability Hierarchy");
    request.summary = chart_view("Hierarchical chart profile demo");
    request.kind = kind == 27u ? FW_HIERARCHICAL_CHART_TREEMAP :
        (kind == 28u ? FW_HIERARCHICAL_CHART_SUNBURST :
            FW_HIERARCHICAL_CHART_PACKED_BUBBLE);
    request.nodes = nodes;
    request.node_count = 7u;
    request.opacity = opacity;
    request.intrinsic_size = (fw_size_f32){640.0f, 360.0f};
    request.transform.struct_size = sizeof(request.transform);
    request.transform.fit = FW_VISUAL_FIT_CONTAIN;
    request.transform.alignment_x = 0.5f;
    request.transform.alignment_y = 0.5f;
    request.transform.clip = 1u;
    request.transform.content_rotation_quarter_turns = rotation;
    request.style.struct_size = sizeof(request.style);
    request.style.show_labels = labels == FW_CHART_LABEL_NONE ? 0u : 1u;
    request.style.show_values = 0u;
    request.style.max_visible_labels = labels == FW_CHART_LABEL_IMPORTANT ?
        3u : 12u;
    request.style.gap = 0.006f;
    request.style.inner_radius = 0.28f;
    request.style.label_scale = 1.0f;
    request.budget.struct_size = sizeof(request.budget);
    request.constraints.struct_size = sizeof(request.constraints);
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.presentation_revision = 1u;
    memset(&state, 0, sizeof(state));
    sink = (fw_chart_draw_sink_v1){sizeof(sink), &state, chart_begin,
        chart_end, chart_rect, chart_line, chart_circle, chart_sector,
        chart_polygon, chart_label};
    services = (fw_chart_services_v1){sizeof(services), &sink, 0u};
    memset(&render, 0, sizeof(render));
    render.struct_size = sizeof(render);
    status = context->hierarchy->render(context->hierarchy_handle, &request,
        (fw_rect_f32){0.0f, 0.0f, width, height}, &services, &render);
    if (status != FW_STATUS_OK) return FWCHART_STATUS_PLUGIN_ERROR;
    memset(&semantics, 0, sizeof(semantics));
    semantics.struct_size = sizeof(semantics);
    status = context->hierarchy->build_semantics(context->hierarchy_handle,
        &request, (fw_rect_f32){0.0f, 0.0f, width, height}, &semantics);
    if (status != FW_STATUS_OK) return FWCHART_STATUS_PLUGIN_ERROR;
    memset(&elements, 0, sizeof(elements));
    return chart_serialize(
        "org.facetwire.reference.hierarchical-chart-renderer",
        "facetwire.renderer.hierarchical-chart", kind, &render,
        &semantics, &state, &elements, UINT32_MAX,
        out_report_utf8_json);
}

static fwchart_status chart_render_demo_internal(fwchart_context *context,
    float width, float height, uint32_t kind, uint32_t rotation,
    float opacity, uint32_t selected_element_index,
    float element_opacity, float translate_x, float translate_y,
    float uniform_scale, float element_rotation_radians,
    uint32_t promoted, uint32_t accent_color,
    uint32_t theme, uint32_t legend, uint32_t labels,
    uint32_t auto_layout,
    fwchart_buffer *out_report_utf8_json) {
    static const char *category_ids[] = {"q1", "q2", "q3", "q4"};
    static const char *category_labels[] = {"Q1", "Q2", "Q3", "Q4"};
    static const double revenue_values[] = {18.0, 27.0, 23.0, 36.0};
    static const double cost_values[] = {12.0, 15.0, 17.0, 21.0};
    fw_chart_category_v1 categories[4];
    fw_chart_value_v1 revenue[4];
    fw_chart_value_v1 cost[4];
    fw_chart_series_v1 series[2];
    fw_chart_renderer_request_v1 request;
    fwchart_sink_state state;
    fw_chart_draw_sink_v1 sink;
    fw_chart_services_v1 services;
    fw_chart_render_result_v1 render;
    fw_chart_semantics_v1 semantics;
    fwchart_element_state element_state;
    fw_chart_element_enum_sink_v1 element_sink;
    fw_chart_element_enum_result_v1 element_result;
    fw_chart_element_observer_v1 observer;
    fw_chart_element_override_v1 element_override;
    fw_chart_presentation_v1 presentation;
    const fw_chart_element_override_v1 *overrides = NULL;
    size_t override_count = 0u;
    fw_status status;
    size_t i;
    if (context == NULL || context->renderer == NULL ||
        context->elements == NULL || context->presentation == NULL ||
        context->hierarchy == NULL ||
        !chart_buffer_available(out_report_utf8_json) ||
        !isfinite(width) || width <= 0.0f ||
        !isfinite(height) || height <= 0.0f || kind > 29u ||
        rotation > 3u || !isfinite(opacity) || opacity < 0.0f ||
        opacity > 1.0f || !isfinite(element_opacity) ||
        element_opacity < 0.0f || element_opacity > 1.0f ||
        !isfinite(translate_x) || !isfinite(translate_y) ||
        !isfinite(uniform_scale) || uniform_scale <= 0.0f ||
        uniform_scale > 100.0f || !isfinite(element_rotation_radians) ||
        promoted > 1u || accent_color > 1u || theme > 5u ||
        legend > 3u || labels > 3u || auto_layout > 1u)
        return FWCHART_STATUS_INVALID_ARGUMENT;
    if (kind >= 27u) {
        if (selected_element_index != UINT32_MAX)
            return FWCHART_STATUS_INVALID_ARGUMENT;
        return chart_render_hierarchy(context, width, height, kind,
            rotation, opacity, theme, labels, out_report_utf8_json);
    }
    memset(categories, 0, sizeof(categories));
    memset(revenue, 0, sizeof(revenue));
    memset(cost, 0, sizeof(cost));
    for (i = 0u; i < 4u; ++i) {
        categories[i].struct_size = sizeof(categories[i]);
        categories[i].id = chart_view(category_ids[i]);
        categories[i].label = chart_view(category_labels[i]);
        revenue[i].struct_size = sizeof(revenue[i]);
        revenue[i].value = revenue_values[i];
        revenue[i].x = (double)i + 1.0;
        revenue[i].size = revenue_values[i];
        revenue[i].minimum = revenue_values[i] - 8.0;
        revenue[i].quartile1 = revenue_values[i] - 4.0;
        revenue[i].median = revenue_values[i];
        revenue[i].quartile3 = revenue_values[i] + 4.0;
        revenue[i].maximum = revenue_values[i] + 8.0;
        revenue[i].open = revenue_values[i] - 2.0;
        revenue[i].high = revenue_values[i] + 5.0;
        revenue[i].low = revenue_values[i] - 5.0;
        revenue[i].close = revenue_values[i] + (i % 2u == 0u ? 3.0 : -3.0);
        cost[i].struct_size = sizeof(cost[i]);
        cost[i].value = cost_values[i];
        cost[i].x = (double)i + 1.0;
        cost[i].size = cost_values[i];
        cost[i].minimum = cost_values[i] - 5.0;
        cost[i].maximum = cost_values[i] + 5.0;
    }
    memset(series, 0, sizeof(series));
    series[0].struct_size = sizeof(series[0]);
    series[0].id = chart_view("revenue");
    series[0].label = chart_view("Revenue");
    series[0].values = revenue;
    series[0].value_count = 4u;
    series[0].color = (fw_color_rgba_f32){0.18f, 0.45f, 0.92f, 1.0f};
    series[0].visible = 1u;
    series[0].mark = FW_CHART_MARK_BAR;
    series[1].struct_size = sizeof(series[1]);
    series[1].id = chart_view("cost");
    series[1].label = chart_view("Cost");
    series[1].values = cost;
    series[1].value_count = 4u;
    series[1].color = (fw_color_rgba_f32){0.92f, 0.36f, 0.22f, 1.0f};
    series[1].visible = 1u;
    series[1].mark = FW_CHART_MARK_LINE;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.request_id = 1u;
    request.zone_id = chart_view("zone:chart-demo");
    request.chart_id = chart_view("chart:quarterly");
    request.title = chart_view("FacetWire Quarterly Trend");
    request.summary = chart_view("Revenue and cost across four quarters");
    switch (kind) {
    case 1u: request.kind = FW_CHART_LINE; break;
    case 2u: request.kind = FW_CHART_PIE; break;
    case 3u: request.kind = FW_CHART_BAR; break;
    case 4u: case 5u: request.kind = FW_CHART_BAR; break;
    case 6u: case 7u: request.kind = FW_CHART_AREA; break;
    case 8u: request.kind = FW_CHART_SCATTER; break;
    case 9u: request.kind = FW_CHART_BUBBLE; break;
    case 10u: request.kind = FW_CHART_DONUT; break;
    case 11u: request.kind = FW_CHART_RADAR; break;
    case 12u: request.kind = FW_CHART_HEATMAP; break;
    case 13u: request.kind = FW_CHART_GAUGE; break;
    case 14u: request.kind = FW_CHART_BOX_PLOT; break;
    case 15u: request.kind = FW_CHART_HISTOGRAM; break;
    case 16u: request.kind = FW_CHART_WATERFALL; break;
    case 17u: request.kind = FW_CHART_FUNNEL; break;
    case 18u: request.kind = FW_CHART_CANDLESTICK; break;
    case 19u: request.kind = FW_CHART_TIME_SERIES; break;
    case 20u: request.kind = FW_CHART_COMBO; break;
    case 21u: request.kind = FW_CHART_DIVERGING_BAR; break;
    case 22u: request.kind = FW_CHART_FACET_LINE; break;
    case 23u: request.kind = FW_CHART_RANGE_AREA; break;
    case 24u: request.kind = FW_CHART_DENSITY_HEATMAP; break;
    case 25u: request.kind = FW_CHART_WORD_CLOUD; break;
    case 26u: request.kind = FW_CHART_ROSE; break;
    default: request.kind = FW_CHART_BAR; break;
    }
    request.categories = categories;
    request.category_count = 4u;
    request.series = series;
    request.series_count = (request.kind == FW_CHART_PIE ||
        request.kind == FW_CHART_DONUT || request.kind == FW_CHART_GAUGE ||
        request.kind == FW_CHART_BOX_PLOT ||
        request.kind == FW_CHART_HISTOGRAM ||
        request.kind == FW_CHART_WATERFALL ||
        request.kind == FW_CHART_FUNNEL ||
        request.kind == FW_CHART_CANDLESTICK ||
        request.kind == FW_CHART_WORD_CLOUD ||
        request.kind == FW_CHART_ROSE) ? 1u : 2u;
    request.opacity = opacity;
    request.intrinsic_size = (fw_size_f32){640.0f, 360.0f};
    request.transform.struct_size = sizeof(request.transform);
    request.transform.fit = FW_VISUAL_FIT_CONTAIN;
    request.transform.alignment_x = 0.5f;
    request.transform.alignment_y = 0.5f;
    request.transform.clip = 1u;
    request.transform.content_rotation_quarter_turns = rotation;
    request.style.struct_size = sizeof(request.style);
    request.style.show_axes = 1u;
    request.style.show_grid = 1u;
    request.style.show_legend = 1u;
    request.style.show_labels = 1u;
    request.style.show_value_labels = 1u;
    request.style.value_precision = 0u;
    request.style.value_label_mode =
        (request.kind == FW_CHART_PIE || request.kind == FW_CHART_DONUT) ?
        FW_CHART_VALUE_LABEL_VALUE_AND_PERCENT :
        FW_CHART_VALUE_LABEL_VALUE;
    request.style.orientation = kind == 3u ?
        FW_CHART_ORIENTATION_HORIZONTAL :
        FW_CHART_ORIENTATION_VERTICAL;
    request.style.stack_mode = kind == 4u || kind == 7u ?
        FW_CHART_STACK_NORMAL : (kind == 5u ?
            FW_CHART_STACK_PERCENT : FW_CHART_STACK_NONE);
    request.style.fill_opacity = 0.28f;
    request.style.donut_inner_radius = 0.56f;
    request.style.bar_gap_ratio = 0.22f;
    request.style.line_width = 0.006f;
    request.style.point_radius = 0.014f;
    request.style.foreground =
        (fw_color_rgba_f32){0.10f, 0.12f, 0.18f, 1.0f};
    request.style.grid_color =
        (fw_color_rgba_f32){0.72f, 0.76f, 0.84f, 0.65f};
    request.budget.struct_size = sizeof(request.budget);
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.max_width = 640.0f;
    request.constraints.max_height = 360.0f;
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.presentation_revision = 1u;
    if (request.kind == FW_CHART_DIVERGING_BAR) {
        revenue[0].value = -18.0;
        revenue[1].value = -8.0;
    }
    memset(&element_state, 0, sizeof(element_state));
    element_state.api = context->elements;
    element_state.plugin = context->handle;
    element_sink = (fw_chart_element_enum_sink_v1){sizeof(element_sink),
        &element_state, chart_element_visit};
    memset(&element_result, 0, sizeof(element_result));
    element_result.struct_size = sizeof(element_result);
    status = context->elements->enumerate(context->handle, &request,
        &element_sink, &element_result);
    if (status != FW_STATUS_OK) return FWCHART_STATUS_PLUGIN_ERROR;
    if (selected_element_index != UINT32_MAX) {
        if (selected_element_index >= element_state.count)
            return FWCHART_STATUS_INVALID_ARGUMENT;
        memset(&element_override, 0, sizeof(element_override));
        element_override.struct_size = sizeof(element_override);
        element_override.selector =
            element_state.records[selected_element_index].ref;
        element_override.selector.part_index = FW_CHART_ELEMENT_PART_ANY;
        element_override.fields = FW_CHART_OVERRIDE_OPACITY |
            FW_CHART_OVERRIDE_TRANSLATION | FW_CHART_OVERRIDE_SCALE |
            FW_CHART_OVERRIDE_ROTATION | FW_CHART_OVERRIDE_Z_OFFSET |
            FW_CHART_OVERRIDE_PROMOTION;
        element_override.opacity = element_opacity;
        element_override.translation =
            (fw_point_f32){translate_x, translate_y};
        element_override.uniform_scale = uniform_scale;
        element_override.rotation_radians = element_rotation_radians;
        element_override.z_offset = promoted != 0u ? 100 : 0;
        element_override.promotion = promoted != 0u ?
            FW_CHART_ELEMENT_PROMOTED : FW_CHART_ELEMENT_INLINE;
        if (accent_color != 0u) {
            element_override.fields |= FW_CHART_OVERRIDE_COLOR;
            element_override.color =
                (fw_color_rgba_f32){0.94f, 0.45f, 0.16f, 1.0f};
        }
        overrides = &element_override;
        override_count = 1u;
    }
    memset(&state, 0, sizeof(state));
    state.elements = context->elements;
    state.plugin = context->handle;
    sink = (fw_chart_draw_sink_v1){sizeof(sink), &state, chart_begin,
        chart_end, chart_rect, chart_line, chart_circle, chart_sector,
        chart_polygon, chart_label};
    services = (fw_chart_services_v1){sizeof(services), &sink, 0u};
    memset(&render, 0, sizeof(render));
    render.struct_size = sizeof(render);
    observer = (fw_chart_element_observer_v1){sizeof(observer), &state,
        chart_element_observe};
    memset(&presentation, 0, sizeof(presentation));
    presentation.struct_size = sizeof(presentation);
    presentation.theme = theme;
    presentation.legend_placement = legend;
    presentation.label_policy = labels;
    presentation.auto_layout = auto_layout;
    presentation.max_visible_labels = 32u;
    presentation.label_padding = 0.004f;
    presentation.title_scale = 1.16f;
    presentation.label_scale = 0.92f;
    presentation.value_scale = 0.88f;
    presentation.flags = FW_CHART_PRESENTATION_USE_THEME_PALETTE |
        FW_CHART_PRESENTATION_AVOID_COLLISIONS;
    status = context->presentation->render(context->handle, &request,
        &presentation, overrides, override_count,
        (fw_rect_f32){0.0f, 0.0f, width, height}, &services, &observer,
        &render);
    if (status != FW_STATUS_OK) return FWCHART_STATUS_PLUGIN_ERROR;
    memset(&semantics, 0, sizeof(semantics));
    semantics.struct_size = sizeof(semantics);
    status = context->renderer->build_semantics(context->handle, &request,
        (fw_rect_f32){0.0f, 0.0f, width, height}, &semantics);
    if (status != FW_STATUS_OK) return FWCHART_STATUS_PLUGIN_ERROR;
    return chart_serialize("org.facetwire.reference.core-chart-renderer",
        "facetwire.renderer.chart", kind, &render, &semantics, &state,
        &element_state, selected_element_index, out_report_utf8_json);
}

fwchart_status fwchart_render_demo(fwchart_context *context, float width,
    float height, uint32_t kind, uint32_t rotation, float opacity,
    fwchart_buffer *out_report_utf8_json) {
    return chart_render_demo_internal(context, width, height, kind,
        rotation, opacity, UINT32_MAX, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0u, 0u, FW_CHART_THEME_BUSINESS, FW_CHART_LEGEND_AUTO,
        FW_CHART_LABEL_AUTO, 1u, out_report_utf8_json);
}

fwchart_status fwchart_render_elements_demo(fwchart_context *context,
    float width, float height, uint32_t kind, uint32_t rotation,
    float opacity, uint32_t selected_element_index,
    float element_opacity, float translate_x, float translate_y,
    float uniform_scale, float element_rotation_radians,
    uint32_t promoted, uint32_t accent_color,
    fwchart_buffer *out_report_utf8_json) {
    return chart_render_demo_internal(context, width, height, kind,
        rotation, opacity, selected_element_index, element_opacity,
        translate_x, translate_y, uniform_scale,
        element_rotation_radians, promoted, accent_color,
        FW_CHART_THEME_BUSINESS, FW_CHART_LEGEND_AUTO,
        FW_CHART_LABEL_AUTO, 1u, out_report_utf8_json);
}

fwchart_status fwchart_render_presentation_demo(fwchart_context *context,
    float width, float height, uint32_t kind, uint32_t rotation,
    float opacity, uint32_t theme, uint32_t legend, uint32_t labels,
    uint32_t auto_layout, fwchart_buffer *out_report_utf8_json) {
    return chart_render_demo_internal(context, width, height, kind,
        rotation, opacity, UINT32_MAX, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0u, 0u, theme, legend, labels, auto_layout,
        out_report_utf8_json);
}

fwchart_status fwchart_render_presentation_elements_demo(
    fwchart_context *context, float width, float height, uint32_t kind,
    uint32_t rotation, float opacity, uint32_t selected_element_index,
    float element_opacity, float translate_x, float translate_y,
    float uniform_scale, float element_rotation_radians,
    uint32_t promoted, uint32_t accent_color, uint32_t theme,
    uint32_t legend, uint32_t labels, uint32_t auto_layout,
    fwchart_buffer *out_report_utf8_json) {
    return chart_render_demo_internal(context, width, height, kind,
        rotation, opacity, selected_element_index, element_opacity,
        translate_x, translate_y, uniform_scale,
        element_rotation_radians, promoted, accent_color, theme, legend,
        labels, auto_layout, out_report_utf8_json);
}

void fwchart_buffer_release(fwchart_buffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->length = 0u;
    }
}
