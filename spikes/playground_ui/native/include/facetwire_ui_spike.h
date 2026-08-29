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

typedef enum fwui_flow_page_mode {
    FWUI_FLOW_PAGE_CONTINUOUS = 0,
    FWUI_FLOW_PAGE_VIRTUAL = 1
} fwui_flow_page_mode;

/* Output buffers must be zero-initialized and released before reuse. An API
 * call never overwrites a non-empty buffer. */

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

/* Legacy compatibility entry point. Cases 0..2 are continuous levels and
 * case 3 is virtual-pages level 1. New hosts must use v2 so content identity
 * and pagination mode cannot be conflated. */
FWUI_API fwui_status fwui_compose_flow_demo(
    fwui_context *context,
    float width,
    float height,
    uint32_t demo_case,
    fwui_buffer *out_layout_plan_utf8_json);
/* Composes content_case 0..2 in the requested fwui_flow_page_mode. */
FWUI_API fwui_status fwui_compose_flow_demo_v2(
    fwui_context *context,
    float width,
    float height,
    uint32_t content_case,
    uint32_t page_mode,
    fwui_buffer *out_layout_plan_utf8_json);
FWUI_API void fwui_buffer_release(fwui_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
