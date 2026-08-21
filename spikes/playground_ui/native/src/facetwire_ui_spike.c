/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_ui_spike.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FWDL_HEADER_SIZE 12u
#define FWDL_COMMAND_SIZE 40u
#define FWDL_COMMAND_COUNT 3u

struct fwui_context {
    uint32_t abi_version;
};

static void clear_buffer(fwui_buffer *buffer) {
    if (buffer != NULL) {
        buffer->data = NULL;
        buffer->length = 0u;
    }
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

static fwui_status copy_text(const char *text, fwui_buffer *out_buffer) {
    size_t length = 0u;
    uint8_t *copy = NULL;
    if (text == NULL || out_buffer == NULL) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }
    length = strlen(text);
    copy = (uint8_t *)calloc(length + 1u, 1u);
    if (copy == NULL && length != 0u) {
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    if (length != 0u) {
        memcpy(copy, text, length);
    }
    out_buffer->data = copy;
    out_buffer->length = (uint64_t)length;
    return FWUI_STATUS_OK;
}

fwui_status fwui_context_create(fwui_context **out_context) {
    fwui_context *context = NULL;
    if (out_context == NULL) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    context = (fwui_context *)calloc(1u, sizeof(*context));
    if (context == NULL) {
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    context->abi_version = 1u;
    *out_context = context;
    return FWUI_STATUS_OK;
}

void fwui_context_destroy(fwui_context *context) {
    free(context);
}

fwui_status fwui_runtime_snapshot(
    fwui_context *context,
    fwui_buffer *out_utf8_json) {
    static const char snapshot[] =
        "{\"abiVersion\":1,\"renderer\":\"placeholder\",\"state\":\"ready\"}";
    clear_buffer(out_utf8_json);
    if (context == NULL || out_utf8_json == NULL || context->abi_version != 1u) {
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
    uint8_t *bytes = NULL;
    fwui_status status = FWUI_STATUS_OK;
    float inset = 0.0f;
    float radius = 0.0f;
    float minimum_dimension = 0.0f;

    clear_buffer(out_display_list);
    clear_buffer(out_semantics_utf8_json);
    if (context == NULL || out_display_list == NULL ||
        out_semantics_utf8_json == NULL ||
        out_display_list == out_semantics_utf8_json || context->abi_version != 1u ||
        !isfinite(width) || !isfinite(height) || !isfinite(opacity) ||
        width <= 0.0f || height <= 0.0f || opacity < 0.0f || opacity > 1.0f) {
        return FWUI_STATUS_INVALID_ARGUMENT;
    }

    minimum_dimension = width < height ? width : height;
    inset = minimum_dimension * 0.25f;
    radius = minimum_dimension * 0.20f;
    if (inset > 12.0f) { inset = 12.0f; }
    if (radius > 12.0f) { radius = 12.0f; }
    bytes = (uint8_t *)calloc((size_t)byte_length, 1u);
    if (bytes == NULL) {
        return FWUI_STATUS_OUT_OF_MEMORY;
    }
    memcpy(bytes, "FWDL", 4u);
    write_u16_le(bytes + 4u, 1u);
    write_u16_le(bytes + 6u, FWDL_HEADER_SIZE);
    write_u32_le(bytes + 8u, FWDL_COMMAND_COUNT);
    write_command(bytes + 12u, 1u, 0.0f, 0.0f, width, height, 0.0f,
        0.07f, 0.09f, 0.13f, opacity);
    write_command(bytes + 52u, 2u, inset, inset, width - (2.0f * inset),
        height - (2.0f * inset), radius, 0.25f, 0.52f, 0.96f, opacity * 0.30f);
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

void fwui_buffer_release(fwui_buffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->length = 0u;
    }
}
