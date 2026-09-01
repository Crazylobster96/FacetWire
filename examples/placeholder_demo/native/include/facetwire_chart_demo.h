/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_CHART_DEMO_H
#define FACETWIRE_CHART_DEMO_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(FWUI_BUILDING_LIBRARY)
#    define FWCHART_API __declspec(dllexport)
#  else
#    define FWCHART_API __declspec(dllimport)
#  endif
#else
#  define FWCHART_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fwchart_context fwchart_context;

typedef struct fwchart_buffer {
    uint8_t *data;
    uint64_t length;
} fwchart_buffer;

typedef enum fwchart_status {
    FWCHART_STATUS_OK = 0,
    FWCHART_STATUS_INVALID_ARGUMENT = 1,
    FWCHART_STATUS_OUT_OF_MEMORY = 2,
    FWCHART_STATUS_PLUGIN_ERROR = 3
} fwchart_status;

/* kind: 0..29, matching the documented Playground chart gallery.
 * rotation: 0..3 quarter turns. */
FWCHART_API fwchart_status fwchart_context_create(
    fwchart_context **out_context);
FWCHART_API void fwchart_context_destroy(fwchart_context *context);
FWCHART_API fwchart_status fwchart_render_demo(
    fwchart_context *context,
    float width,
    float height,
    uint32_t kind,
    uint32_t rotation,
    float opacity,
    fwchart_buffer *out_report_utf8_json);
FWCHART_API fwchart_status fwchart_render_elements_demo(
    fwchart_context *context,
    float width,
    float height,
    uint32_t kind,
    uint32_t rotation,
    float opacity,
    uint32_t selected_element_index,
    float element_opacity,
    float translate_x,
    float translate_y,
    float uniform_scale,
    float element_rotation_radians,
    uint32_t promoted,
    uint32_t accent_color,
    fwchart_buffer *out_report_utf8_json);
/* theme: 0 auto, 1 light, 2 dark, 3 business, 4 academic, 5 contrast.
 * legend: 0 auto, 1 bottom, 2 right, 3 hidden.
 * labels: 0 auto, 1 all, 2 important, 3 none. */
FWCHART_API fwchart_status fwchart_render_presentation_demo(
    fwchart_context *context,
    float width,
    float height,
    uint32_t kind,
    uint32_t rotation,
    float opacity,
    uint32_t theme,
    uint32_t legend,
    uint32_t labels,
    uint32_t auto_layout,
    fwchart_buffer *out_report_utf8_json);
/* Combines presentation policy with one element adjustment.  The legacy
 * elements entry point remains available and keeps its business-theme
 * defaults for ABI compatibility. */
FWCHART_API fwchart_status fwchart_render_presentation_elements_demo(
    fwchart_context *context,
    float width,
    float height,
    uint32_t kind,
    uint32_t rotation,
    float opacity,
    uint32_t selected_element_index,
    float element_opacity,
    float translate_x,
    float translate_y,
    float uniform_scale,
    float element_rotation_radians,
    uint32_t promoted,
    uint32_t accent_color,
    uint32_t theme,
    uint32_t legend,
    uint32_t labels,
    uint32_t auto_layout,
    fwchart_buffer *out_report_utf8_json);
FWCHART_API void fwchart_buffer_release(fwchart_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
