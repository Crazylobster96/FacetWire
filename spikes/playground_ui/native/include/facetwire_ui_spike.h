/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_UI_SPIKE_H
#define FACETWIRE_UI_SPIKE_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(FWUI_BUILDING_LIBRARY)
#    define FWUI_API __declspec(dllexport)
#  else
#    define FWUI_API __declspec(dllimport)
#  endif
#else
#  define FWUI_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum fwui_status {
    FWUI_STATUS_OK = 0,
    FWUI_STATUS_INVALID_ARGUMENT = 1,
    FWUI_STATUS_OUT_OF_MEMORY = 2
} fwui_status;

typedef struct fwui_context fwui_context;

typedef struct fwui_buffer {
    uint8_t *data;
    uint64_t length;
} fwui_buffer;

FWUI_API fwui_status fwui_context_create(fwui_context **out_context);
FWUI_API void fwui_context_destroy(fwui_context *context);
FWUI_API fwui_status fwui_runtime_snapshot(
    fwui_context *context,
    fwui_buffer *out_utf8_json);
FWUI_API fwui_status fwui_render_placeholder(
    fwui_context *context,
    float width,
    float height,
    float opacity,
    fwui_buffer *out_display_list,
    fwui_buffer *out_semantics_utf8_json);
FWUI_API void fwui_buffer_release(fwui_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
