/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_placeholder_demo.h"

#include <facetwire/placeholder_renderer.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FWDEMO_CONTEXT_MAGIC UINT32_C(0x4657444d)
#define FWDEMO_MAX_COMMANDS 64u
#define FWDEMO_MAX_STRING_BYTES 8192u

typedef enum fwdemo_command_kind {
    FWDEMO_COMMAND_SAVE = 1,
    FWDEMO_COMMAND_RESTORE = 2,
    FWDEMO_COMMAND_CLIP_RECT = 3,
    FWDEMO_COMMAND_FILL_ROUNDED_RECT = 4,
    FWDEMO_COMMAND_STROKE_ROUNDED_RECT = 5,
    FWDEMO_COMMAND_SYMBOL = 6,
    FWDEMO_COMMAND_TEXT = 7
} fwdemo_command_kind;

typedef struct fwdemo_command {
    fwdemo_command_kind kind;
    fw_rect_f32 rect;
    float radius;
    float stroke_width;
    fw_color_rgba_f32 color;
    uint32_t dashed;
    char *text;
    size_t text_length;
} fwdemo_command;

typedef struct fwdemo_recording_sink {
    fwdemo_command commands[FWDEMO_MAX_COMMANDS];
    uint32_t command_count;
} fwdemo_recording_sink;

typedef struct fwdemo_text_layout {
    char *text;
    size_t text_length;
    float font_size;
} fwdemo_text_layout;

typedef struct fwdemo_writer {
    char *data;
    size_t length;
    size_t capacity;
    int failed;
} fwdemo_writer;

struct fwdemo_context {
    uint32_t magic;
    const fw_plugin_api_v1 *plugin_api;
    const fw_plugin_descriptor_v1 *descriptor;
    const fw_placeholder_renderer_api_v1 *renderer;
    fw_plugin_handle plugin;
};

static void fwdemo_clear_buffer(fwdemo_buffer *buffer) {
    if (buffer != NULL) {
        buffer->data = NULL;
        buffer->length = 0u;
    }
}

static int fwdemo_context_valid(const fwdemo_context *context) {
    return context != NULL && context->magic == FWDEMO_CONTEXT_MAGIC &&
        context->plugin_api != NULL && context->descriptor != NULL &&
        context->renderer != NULL && context->plugin != NULL;
}

static int fwdemo_writer_reserve(fwdemo_writer *writer, size_t extra) {
    size_t required;
    size_t capacity;
    char *replacement;
    if (writer == NULL || writer->failed) {
        return 0;
    }
    if (extra > SIZE_MAX - writer->length - 1u) {
        writer->failed = 1;
        return 0;
    }
    required = writer->length + extra + 1u;
    if (required <= writer->capacity) {
        return 1;
    }
    capacity = writer->capacity == 0u ? 1024u : writer->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    replacement = (char *)realloc(writer->data, capacity);
    if (replacement == NULL) {
        writer->failed = 1;
        return 0;
    }
    writer->data = replacement;
    writer->capacity = capacity;
    return 1;
}

static int fwdemo_writer_bytes(
    fwdemo_writer *writer,
    const char *bytes,
    size_t length) {
    if (bytes == NULL && length != 0u) {
        writer->failed = 1;
        return 0;
    }
    if (!fwdemo_writer_reserve(writer, length)) {
        return 0;
    }
    if (length != 0u) {
        memcpy(writer->data + writer->length, bytes, length);
    }
    writer->length += length;
    writer->data[writer->length] = '\0';
    return 1;
}

static int fwdemo_writer_text(fwdemo_writer *writer, const char *text) {
    return text != NULL ? fwdemo_writer_bytes(writer, text, strlen(text)) : 0;
}

static int fwdemo_writer_format(
    fwdemo_writer *writer,
    const char *format,
    ...) {
    va_list arguments;
    va_list copy;
    int required;
    int written;
    if (writer == NULL || format == NULL || writer->failed) {
        return 0;
    }
    va_start(arguments, format);
    va_copy(copy, arguments);
    required = vsnprintf(NULL, 0u, format, copy);
    va_end(copy);
    if (required < 0 || !fwdemo_writer_reserve(writer, (size_t)required)) {
        writer->failed = 1;
        va_end(arguments);
        return 0;
    }
    written = vsnprintf(
        writer->data + writer->length,
        writer->capacity - writer->length,
        format,
        arguments);
    va_end(arguments);
    if (written != required) {
        writer->failed = 1;
        return 0;
    }
    writer->length += (size_t)written;
    return 1;
}

static int fwdemo_writer_json_string(
    fwdemo_writer *writer,
    const char *bytes,
    size_t length) {
    size_t index;
    if (!fwdemo_writer_text(writer, "\"")) {
        return 0;
    }
    for (index = 0u; index < length; ++index) {
        const unsigned char value = (unsigned char)bytes[index];
        if (value == '"' || value == '\\') {
            const char escaped[2] = {'\\', (char)value};
            if (!fwdemo_writer_bytes(writer, escaped, sizeof(escaped))) {
                return 0;
            }
        } else if (value <= 0x1fu) {
            if (!fwdemo_writer_format(writer, "\\u%04x", (unsigned int)value)) {
                return 0;
            }
        } else if (!fwdemo_writer_bytes(writer, (const char *)&bytes[index], 1u)) {
            return 0;
        }
    }
    return fwdemo_writer_text(writer, "\"");
}

static int fwdemo_writer_view(
    fwdemo_writer *writer,
    fw_string_view value) {
    return fwdemo_writer_json_string(writer, value.data, value.length);
}

static int32_t fwdemo_writer_finish(
    fwdemo_writer *writer,
    fwdemo_buffer *output) {
    if (writer == NULL || output == NULL || writer->failed) {
        if (writer != NULL) {
            free(writer->data);
            writer->data = NULL;
        }
        return (int32_t)FW_STATUS_OUT_OF_MEMORY;
    }
    output->data = (uint8_t *)writer->data;
    output->length = (uint64_t)writer->length;
    writer->data = NULL;
    writer->length = 0u;
    writer->capacity = 0u;
    return (int32_t)FW_STATUS_OK;
}

static void fwdemo_sink_clear(fwdemo_recording_sink *sink) {
    uint32_t index;
    if (sink == NULL) {
        return;
    }
    for (index = 0u; index < sink->command_count; ++index) {
        free(sink->commands[index].text);
        sink->commands[index].text = NULL;
    }
    memset(sink, 0, sizeof(*sink));
}

static fw_status fwdemo_record(
    fwdemo_recording_sink *sink,
    fwdemo_command_kind kind,
    fwdemo_command **out_command) {
    fwdemo_command *command;
    if (sink == NULL || out_command == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (sink->command_count >= FWDEMO_MAX_COMMANDS) {
        return FW_STATUS_RESOURCE_LIMIT;
    }
    command = &sink->commands[sink->command_count++];
    memset(command, 0, sizeof(*command));
    command->kind = kind;
    *out_command = command;
    return FW_STATUS_OK;
}

static fw_status fwdemo_record_text(
    fwdemo_command *command,
    fw_string_view value) {
    if (command == NULL || (value.data == NULL && value.length != 0u)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (value.length == 0u) {
        return FW_STATUS_OK;
    }
    command->text = (char *)malloc(value.length);
    if (command->text == NULL) {
        return FW_STATUS_OUT_OF_MEMORY;
    }
    memcpy(command->text, value.data, value.length);
    command->text_length = value.length;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fwdemo_save(void *user_data) {
    fwdemo_command *command = NULL;
    return fwdemo_record(
        (fwdemo_recording_sink *)user_data, FWDEMO_COMMAND_SAVE, &command);
}

static fw_status FW_CALL fwdemo_restore(void *user_data) {
    fwdemo_command *command = NULL;
    return fwdemo_record(
        (fwdemo_recording_sink *)user_data, FWDEMO_COMMAND_RESTORE, &command);
}

static fw_status FW_CALL fwdemo_clip(void *user_data, fw_rect_f32 rect) {
    fwdemo_command *command = NULL;
    fw_status status = fwdemo_record(
        (fwdemo_recording_sink *)user_data,
        FWDEMO_COMMAND_CLIP_RECT,
        &command);
    if (status == FW_STATUS_OK) {
        command->rect = rect;
    }
    return status;
}

static fw_status FW_CALL fwdemo_fill(
    void *user_data,
    fw_rect_f32 rect,
    float radius,
    fw_color_rgba_f32 color) {
    fwdemo_command *command = NULL;
    fw_status status = fwdemo_record(
        (fwdemo_recording_sink *)user_data,
        FWDEMO_COMMAND_FILL_ROUNDED_RECT,
        &command);
    if (status == FW_STATUS_OK) {
        command->rect = rect;
        command->radius = radius;
        command->color = color;
    }
    return status;
}

static fw_status FW_CALL fwdemo_stroke(
    void *user_data,
    fw_rect_f32 rect,
    float radius,
    const fw_stroke_style_v1 *style) {
    fwdemo_command *command = NULL;
    fw_status status;
    if (style == NULL || style->struct_size < sizeof(*style)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    status = fwdemo_record(
        (fwdemo_recording_sink *)user_data,
        FWDEMO_COMMAND_STROKE_ROUNDED_RECT,
        &command);
    if (status == FW_STATUS_OK) {
        command->rect = rect;
        command->radius = radius;
        command->stroke_width = style->width;
        command->color = style->color;
        command->dashed = style->dashed;
    }
    return status;
}

static fw_status FW_CALL fwdemo_symbol(
    void *user_data,
    fw_string_view symbol_id,
    fw_rect_f32 rect,
    fw_color_rgba_f32 color) {
    fwdemo_command *command = NULL;
    fw_status status = fwdemo_record(
        (fwdemo_recording_sink *)user_data,
        FWDEMO_COMMAND_SYMBOL,
        &command);
    if (status == FW_STATUS_OK) {
        command->rect = rect;
        command->color = color;
        status = fwdemo_record_text(command, symbol_id);
    }
    return status;
}

static fw_status FW_CALL fwdemo_draw_text(
    void *user_data,
    fw_text_layout_handle layout_handle,
    fw_point_f32 origin,
    fw_color_rgba_f32 color) {
    const fwdemo_text_layout *layout =
        (const fwdemo_text_layout *)layout_handle;
    fwdemo_command *command = NULL;
    fw_status status;
    fw_string_view text;
    if (layout == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    status = fwdemo_record(
        (fwdemo_recording_sink *)user_data,
        FWDEMO_COMMAND_TEXT,
        &command);
    if (status != FW_STATUS_OK) {
        return status;
    }
    command->rect.x = origin.x;
    command->rect.y = origin.y;
    command->rect.height = layout->font_size;
    command->color = color;
    text.data = layout->text;
    text.length = layout->text_length;
    return fwdemo_record_text(command, text);
}

static fw_status FW_CALL fwdemo_layout_text(
    void *user_data,
    const fw_text_layout_request_v1 *request,
    fw_text_layout_handle *out_layout,
    fw_text_layout_metrics_v1 *out_metrics) {
    fwdemo_text_layout *layout;
    float estimated_width;
    (void)user_data;
    if (request == NULL || out_layout == NULL || out_metrics == NULL ||
        request->struct_size < sizeof(*request) ||
        out_metrics->struct_size < sizeof(*out_metrics) ||
        (request->text.data == NULL && request->text.length != 0u) ||
        request->text.length > FWDEMO_MAX_STRING_BYTES) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_layout = NULL;
    layout = (fwdemo_text_layout *)calloc(1u, sizeof(*layout));
    if (layout == NULL) {
        return FW_STATUS_OUT_OF_MEMORY;
    }
    if (request->text.length != 0u) {
        layout->text = (char *)malloc(request->text.length);
        if (layout->text == NULL) {
            free(layout);
            return FW_STATUS_OUT_OF_MEMORY;
        }
        memcpy(layout->text, request->text.data, request->text.length);
    }
    layout->text_length = request->text.length;
    layout->font_size = request->font_size;
    estimated_width = (float)request->text.length * request->font_size * 0.55f;
    if (estimated_width > request->max_width) {
        estimated_width = request->max_width;
        out_metrics->did_truncate = 1u;
    }
    out_metrics->size.width = estimated_width;
    out_metrics->size.height = request->font_size * 1.25f;
    out_metrics->baseline = request->font_size;
    out_metrics->line_count = request->text.length == 0u ? 0u : 1u;
    *out_layout = (fw_text_layout_handle)layout;
    return FW_STATUS_OK;
}

static void FW_CALL fwdemo_release_text(
    void *user_data,
    fw_text_layout_handle layout_handle) {
    fwdemo_text_layout *layout = (fwdemo_text_layout *)layout_handle;
    (void)user_data;
    if (layout != NULL) {
        free(layout->text);
        free(layout);
    }
}

static int fwdemo_input_string_valid(const char *data, uint64_t length) {
    return length <= FWDEMO_MAX_STRING_BYTES &&
        length <= (uint64_t)SIZE_MAX && (data != NULL || length == 0u);
}

static fw_status fwdemo_make_placeholder_request(
    const fwdemo_request_v1 *input,
    fw_placeholder_request_v1 *output) {
    const int dark = input != NULL && input->prefers_dark != 0u;
    if (input == NULL || output == NULL ||
        input->struct_size < sizeof(*input) ||
        !fwdemo_input_string_valid(
            input->content_kind_utf8, input->content_kind_length) ||
        !fwdemo_input_string_valid(input->label_utf8, input->label_length)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    output->struct_size = sizeof(*output);
    output->request_id = 1u;
    output->zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:demo");
    output->content_kind.data = input->content_kind_utf8;
    output->content_kind.length = (size_t)input->content_kind_length;
    output->required_capability_id =
        (fw_string_view)FW_STRING_VIEW_LITERAL("facetwire.renderer.demo");
    output->accessible_label.data = input->label_utf8;
    output->accessible_label.length = (size_t)input->label_length;
    output->diagnostic_code =
        (fw_string_view)FW_STRING_VIEW_LITERAL("DEMO-CAPABILITY-UNAVAILABLE");
    output->reason = input->reason;
    output->mode = input->mode;
    output->permitted_actions = input->permitted_actions;
    output->constraints.struct_size = sizeof(output->constraints);
    output->constraints.max_width = 10000.0f;
    output->constraints.max_height = 10000.0f;
    output->constraints.em_size = 16.0f;
    output->constraints.line_height = 19.2f;
    switch (input->measure_case) {
    case 1u:
        output->resolved_size.has_value = 1u;
        output->resolved_size.value.width = input->width;
        output->resolved_size.value.height = input->height;
        break;
    case 2u:
        output->intrinsic_size.has_value = 1u;
        output->intrinsic_size.value.width = input->width * 0.75f;
        output->intrinsic_size.value.height = input->height * 0.75f;
        break;
    case 3u:
        output->constraints.min_width = input->width;
        output->constraints.max_width = input->width;
        output->intrinsic_aspect_ratio.has_value = 1u;
        output->intrinsic_aspect_ratio.value = 16.0f / 9.0f;
        break;
    case 4u:
        output->constraints.min_width = input->width;
        output->constraints.max_width = input->width;
        output->constraints.min_height = input->height;
        output->constraints.max_height = input->height;
        break;
    default:
        break;
    }
    output->style.struct_size = sizeof(output->style);
    output->style.background = (fw_color_rgba_f32){
        dark ? 0.08f : 0.93f,
        dark ? 0.10f : 0.95f,
        dark ? 0.15f : 1.00f,
        input->background_alpha};
    output->style.border = (fw_color_rgba_f32){
        dark ? 0.54f : 0.18f, dark ? 0.70f : 0.38f, 0.96f, 1.0f};
    output->style.icon = (fw_color_rgba_f32){
        dark ? 0.68f : 0.12f, dark ? 0.80f : 0.34f, 1.0f, 1.0f};
    output->style.primary_text = (fw_color_rgba_f32){
        dark ? 0.96f : 0.08f,
        dark ? 0.97f : 0.10f,
        dark ? 1.00f : 0.15f,
        1.0f};
    output->style.secondary_text = output->style.primary_text;
    output->style.action = (fw_color_rgba_f32){0.19f, 0.45f, 0.90f, 0.88f};
    output->style.opacity = input->opacity;
    output->style.border_width = input->high_contrast != 0u ? 3.0f : 1.5f;
    output->style.corner_radius = 18.0f;
    output->style.content_padding = 16.0f;
    output->style.gap = 10.0f;
    output->style.icon_size = 32.0f;
    output->target.struct_size = sizeof(output->target);
    output->target.device_pixel_ratio = input->device_pixel_ratio;
    output->target.font_scale = input->font_scale;
    output->target.medium = FW_RENDER_MEDIUM_SCREEN;
    output->target.prefers_dark = input->prefers_dark;
    output->target.high_contrast = input->high_contrast;
    output->target.reduce_motion = input->reduce_motion;
    output->target.supports_alpha = 1u;
    output->fragment_count = 1u;
    output->presentation_revision = input->presentation_revision;
    output->phase = input->phase;
    output->progress.struct_size = sizeof(output->progress);
    output->progress.kind = input->progress_kind;
    output->progress.completed = input->completed;
    output->progress.total = input->total;
    output->stale = input->stale;
    return FW_STATUS_OK;
}

static const char *fwdemo_command_name(fwdemo_command_kind kind) {
    switch (kind) {
    case FWDEMO_COMMAND_SAVE: return "save";
    case FWDEMO_COMMAND_RESTORE: return "restore";
    case FWDEMO_COMMAND_CLIP_RECT: return "clipRect";
    case FWDEMO_COMMAND_FILL_ROUNDED_RECT: return "fillRoundedRect";
    case FWDEMO_COMMAND_STROKE_ROUNDED_RECT: return "strokeRoundedRect";
    case FWDEMO_COMMAND_SYMBOL: return "symbol";
    case FWDEMO_COMMAND_TEXT: return "text";
    default: return "unknown";
    }
}

static int fwdemo_write_command(
    fwdemo_writer *writer,
    const fwdemo_command *command) {
    if (!fwdemo_writer_format(
            writer,
            "{\"op\":\"%s\",\"x\":%.9g,\"y\":%.9g,"
            "\"width\":%.9g,\"height\":%.9g,\"radius\":%.9g,"
            "\"strokeWidth\":%.9g,\"dashed\":%u,"
            "\"red\":%.9g,\"green\":%.9g,\"blue\":%.9g,"
            "\"alpha\":%.9g,\"value\":",
            fwdemo_command_name(command->kind),
            (double)command->rect.x,
            (double)command->rect.y,
            (double)command->rect.width,
            (double)command->rect.height,
            (double)command->radius,
            (double)command->stroke_width,
            command->dashed,
            (double)command->color.red,
            (double)command->color.green,
            (double)command->color.blue,
            (double)command->color.alpha)) {
        return 0;
    }
    if (!fwdemo_writer_json_string(
            writer, command->text, command->text_length)) {
        return 0;
    }
    return fwdemo_writer_text(writer, "}");
}

int32_t FWDEMO_CALL fwdemo_context_create(fwdemo_context **out_context) {
    const fw_plugin_api_v1 *plugin_api;
    const void *interface_value = NULL;
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fwdemo_context *context;
    fw_status status;
    if (out_context == NULL) {
        return (int32_t)FW_STATUS_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    plugin_api = facetwire_plugin_query(FW_ABI_VERSION_CURRENT);
    if (plugin_api == NULL || plugin_api->get_descriptor == NULL ||
        plugin_api->load == NULL || plugin_api->unload == NULL ||
        plugin_api->query_interface == NULL) {
        return (int32_t)FW_STATUS_INVALID_PLUGIN;
    }
    context = (fwdemo_context *)calloc(1u, sizeof(*context));
    if (context == NULL) {
        return (int32_t)FW_STATUS_OUT_OF_MEMORY;
    }
    context->plugin_api = plugin_api;
    context->descriptor = plugin_api->get_descriptor();
    status = plugin_api->load(&host, &context->plugin);
    if (status == FW_STATUS_OK) {
        status = plugin_api->query_interface(
            context->plugin,
            (fw_string_view)FW_STRING_VIEW_LITERAL(
                FW_PLACEHOLDER_RENDERER_INTERFACE_ID),
            FW_PLACEHOLDER_RENDERER_INTERFACE_VERSION,
            &interface_value);
    }
    if (status != FW_STATUS_OK || interface_value == NULL) {
        if (context->plugin != NULL) {
            plugin_api->unload(context->plugin);
        }
        free(context);
        return (int32_t)(status == FW_STATUS_OK ?
            FW_STATUS_INVALID_PLUGIN : status);
    }
    context->renderer =
        (const fw_placeholder_renderer_api_v1 *)interface_value;
    context->magic = FWDEMO_CONTEXT_MAGIC;
    *out_context = context;
    return (int32_t)FW_STATUS_OK;
}

void FWDEMO_CALL fwdemo_context_destroy(fwdemo_context *context) {
    if (!fwdemo_context_valid(context)) {
        return;
    }
    context->magic = 0u;
    context->plugin_api->unload(context->plugin);
    context->plugin = NULL;
    free(context);
}

int32_t FWDEMO_CALL fwdemo_runtime_snapshot(
    fwdemo_context *context,
    fwdemo_buffer *out_utf8_json) {
    fwdemo_writer writer = {0};
    const fw_capability_descriptor_v1 *capability;
    fwdemo_clear_buffer(out_utf8_json);
    if (!fwdemo_context_valid(context) || out_utf8_json == NULL ||
        context->descriptor->capability_count == 0u) {
        return (int32_t)FW_STATUS_INVALID_ARGUMENT;
    }
    capability = &context->descriptor->capabilities[0];
    fwdemo_writer_format(
        &writer,
        "{\"abiVersion\":\"%u.%u\",\"pluginId\":",
        context->descriptor->abi_version.major,
        context->descriptor->abi_version.minor);
    fwdemo_writer_view(&writer, context->descriptor->id);
    fwdemo_writer_text(&writer, ",\"pluginName\":");
    fwdemo_writer_view(&writer, context->descriptor->name);
    fwdemo_writer_text(&writer, ",\"pluginVersion\":");
    fwdemo_writer_view(&writer, context->descriptor->version);
    fwdemo_writer_text(&writer, ",\"capabilityId\":");
    fwdemo_writer_view(&writer, capability->id);
    fwdemo_writer_format(
        &writer,
        ",\"interfaceVersion\":%u,\"state\":\"ready\"}",
        context->renderer->interface_version);
    return fwdemo_writer_finish(&writer, out_utf8_json);
}

int32_t FWDEMO_CALL fwdemo_parameter_schema(
    fwdemo_context *context,
    fwdemo_buffer *out_utf8_json) {
    fw_string_view schema = {0};
    fw_status status;
    fwdemo_clear_buffer(out_utf8_json);
    if (!fwdemo_context_valid(context) || out_utf8_json == NULL) {
        return (int32_t)FW_STATUS_INVALID_ARGUMENT;
    }
    status = context->renderer->get_parameter_schema(context->plugin, &schema);
    if (status != FW_STATUS_OK) {
        return (int32_t)status;
    }
    out_utf8_json->data = (uint8_t *)malloc(schema.length + 1u);
    if (out_utf8_json->data == NULL) {
        return (int32_t)FW_STATUS_OUT_OF_MEMORY;
    }
    if (schema.length != 0u) {
        memcpy(out_utf8_json->data, schema.data, schema.length);
    }
    out_utf8_json->data[schema.length] = 0u;
    out_utf8_json->length = (uint64_t)schema.length;
    return (int32_t)FW_STATUS_OK;
}

int32_t FWDEMO_CALL fwdemo_render(
    fwdemo_context *context,
    const fwdemo_request_v1 *input,
    fwdemo_buffer *out_utf8_json) {
    fw_placeholder_request_v1 request;
    fw_placeholder_validation_result_v1 validation = {0};
    fw_placeholder_measure_result_v1 measure = {0};
    fw_placeholder_render_result_v1 render = {0};
    fw_placeholder_semantics_v1 semantics = {0};
    fwdemo_recording_sink sink_state = {0};
    fw_display_list_sink_v1 sink = {0};
    fw_text_service_v1 text_service = {0};
    fw_placeholder_services_v1 services = {0};
    fw_rect_f32 bounds;
    fw_status status;
    fwdemo_writer writer = {0};
    uint32_t index;
    fwdemo_clear_buffer(out_utf8_json);
    if (!fwdemo_context_valid(context) || out_utf8_json == NULL) {
        return (int32_t)FW_STATUS_INVALID_ARGUMENT;
    }
    status = fwdemo_make_placeholder_request(input, &request);
    if (status != FW_STATUS_OK) {
        return (int32_t)status;
    }
    validation.struct_size = sizeof(validation);
    status = context->renderer->validate(
        context->plugin, &request, &validation);
    if (status != FW_STATUS_OK) {
        return (int32_t)status;
    }
    measure.struct_size = sizeof(measure);
    status = context->renderer->measure(context->plugin, &request, &measure);
    if (status != FW_STATUS_OK) {
        return (int32_t)status;
    }
    bounds = (fw_rect_f32){0.0f, 0.0f, input->width, input->height};
    sink.struct_size = sizeof(sink);
    sink.user_data = &sink_state;
    sink.save = fwdemo_save;
    sink.restore = fwdemo_restore;
    sink.clip_rect = fwdemo_clip;
    sink.fill_rounded_rect = fwdemo_fill;
    sink.stroke_rounded_rect = fwdemo_stroke;
    sink.draw_symbol = fwdemo_symbol;
    sink.draw_text_layout = fwdemo_draw_text;
    text_service.struct_size = sizeof(text_service);
    text_service.layout_utf8 = fwdemo_layout_text;
    text_service.release_layout = fwdemo_release_text;
    services.struct_size = sizeof(services);
    services.display_list = &sink;
    services.text = &text_service;
    services.locale = (fw_string_view)FW_STRING_VIEW_LITERAL("zh-CN");
    render.struct_size = sizeof(render);
    status = context->renderer->render(
        context->plugin, &request, bounds, &services, &render);
    if (status != FW_STATUS_OK) {
        fwdemo_sink_clear(&sink_state);
        return (int32_t)status;
    }
    semantics.struct_size = sizeof(semantics);
    status = context->renderer->build_semantics(
        context->plugin, &request, bounds, &semantics);
    if (status != FW_STATUS_OK) {
        fwdemo_sink_clear(&sink_state);
        return (int32_t)status;
    }

    fwdemo_writer_format(
        &writer,
        "{\"contract\":{\"validationStatus\":%d,"
        "\"normalizationFlags\":%u},\"measure\":{"
        "\"width\":%.9g,\"height\":%.9g,\"source\":%u,"
        "\"normalizationFlags\":%u},\"render\":{"
        "\"commandCount\":%u,\"visualDensity\":%u,"
        "\"visibleActions\":%u,\"normalizationFlags\":%u,"
        "\"cacheKeyHigh\":\"%llu\",\"cacheKeyLow\":\"%llu\"},"
        "\"semantics\":{\"role\":%u,\"label\":",
        (int)validation.status,
        validation.normalization_flags,
        (double)measure.size.width,
        (double)measure.size.height,
        measure.source,
        measure.normalization_flags,
        render.emitted_command_count,
        render.visual_density,
        render.visible_actions,
        render.normalization_flags,
        (unsigned long long)render.cache_key_high,
        (unsigned long long)render.cache_key_low,
        semantics.role);
    fwdemo_writer_view(&writer, semantics.accessible_label);
    fwdemo_writer_text(&writer, ",\"statusKey\":");
    fwdemo_writer_view(&writer, semantics.status_localization_key);
    fwdemo_writer_format(
        &writer,
        ",\"availableActions\":%u,\"hidden\":%u,\"stale\":%u,"
        "\"phase\":%u},\"commands\":[",
        semantics.available_actions,
        semantics.hidden_visually,
        semantics.stale,
        semantics.phase);
    for (index = 0u; index < sink_state.command_count; ++index) {
        if (index != 0u) {
            fwdemo_writer_text(&writer, ",");
        }
        fwdemo_write_command(&writer, &sink_state.commands[index]);
    }
    fwdemo_writer_text(&writer, "]}");
    fwdemo_sink_clear(&sink_state);
    return fwdemo_writer_finish(&writer, out_utf8_json);
}

int32_t FWDEMO_CALL fwdemo_hit_test(
    fwdemo_context *context,
    const fwdemo_request_v1 *input,
    float x,
    float y,
    uint32_t *out_hit,
    uint32_t *out_action) {
    fw_placeholder_hit_test_request_v1 request;
    fw_placeholder_hit_test_result_v1 result = {0};
    fw_status status;
    if (!fwdemo_context_valid(context) || out_hit == NULL ||
        out_action == NULL) {
        return (int32_t)FW_STATUS_INVALID_ARGUMENT;
    }
    *out_hit = 0u;
    *out_action = 0u;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    status = fwdemo_make_placeholder_request(input, &request.placeholder);
    if (status != FW_STATUS_OK) {
        return (int32_t)status;
    }
    request.bounds = (fw_rect_f32){0.0f, 0.0f, input->width, input->height};
    request.point = (fw_point_f32){x, y};
    result.struct_size = sizeof(result);
    status = context->renderer->hit_test(context->plugin, &request, &result);
    if (status == FW_STATUS_OK) {
        *out_hit = result.hit;
        *out_action = result.action;
    }
    return (int32_t)status;
}

void FWDEMO_CALL fwdemo_buffer_release(fwdemo_buffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->length = 0u;
    }
}
