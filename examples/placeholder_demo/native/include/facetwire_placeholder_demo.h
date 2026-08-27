/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_PLACEHOLDER_DEMO_H
#define FACETWIRE_PLACEHOLDER_DEMO_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define FWDEMO_API __declspec(dllexport)
#define FWDEMO_CALL __cdecl
#else
#define FWDEMO_API __attribute__((visibility("default")))
#define FWDEMO_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fwdemo_buffer {
    uint8_t *data;
    uint64_t length;
} fwdemo_buffer;

typedef struct fwdemo_request_v1 {
    uint32_t struct_size;
    float width;
    float height;
    float opacity;
    float background_alpha;
    float font_scale;
    float device_pixel_ratio;
    uint32_t reason;
    uint32_t mode;
    uint32_t permitted_actions;
    uint32_t phase;
    uint32_t progress_kind;
    uint32_t stale;
    uint32_t prefers_dark;
    uint32_t high_contrast;
    uint32_t reduce_motion;
    uint32_t measure_case;
    uint64_t completed;
    uint64_t total;
    uint64_t presentation_revision;
    const char *content_kind_utf8;
    uint64_t content_kind_length;
    const char *label_utf8;
    uint64_t label_length;
} fwdemo_request_v1;

typedef struct fwdemo_context fwdemo_context;

/* Output buffers must be zero-initialized. Release a successful output with
 * fwdemo_buffer_release before reusing the same buffer. */

FWDEMO_API int32_t FWDEMO_CALL fwdemo_context_create(
    fwdemo_context **out_context);
FWDEMO_API void FWDEMO_CALL fwdemo_context_destroy(fwdemo_context *context);
FWDEMO_API int32_t FWDEMO_CALL fwdemo_runtime_snapshot(
    fwdemo_context *context,
    fwdemo_buffer *out_utf8_json);
FWDEMO_API int32_t FWDEMO_CALL fwdemo_parameter_schema(
    fwdemo_context *context,
    fwdemo_buffer *out_utf8_json);
FWDEMO_API int32_t FWDEMO_CALL fwdemo_render(
    fwdemo_context *context,
    const fwdemo_request_v1 *request,
    fwdemo_buffer *out_utf8_json);
FWDEMO_API int32_t FWDEMO_CALL fwdemo_hit_test(
    fwdemo_context *context,
    const fwdemo_request_v1 *request,
    float x,
    float y,
    uint32_t *out_hit,
    uint32_t *out_action);
FWDEMO_API void FWDEMO_CALL fwdemo_buffer_release(fwdemo_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif

