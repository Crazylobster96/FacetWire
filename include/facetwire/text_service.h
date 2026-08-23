/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_TEXT_SERVICE_H
#define FACETWIRE_TEXT_SERVICE_H

#include <facetwire/display_list.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_text_layout_request_v1 {
    uint32_t struct_size;
    fw_string_view text;
    fw_string_view locale;
    float font_size;
    float max_width;
    uint32_t max_lines;
    uint32_t direction;
    uint32_t ellipsize;
    uint32_t flags;
} fw_text_layout_request_v1;

typedef struct fw_text_layout_metrics_v1 {
    uint32_t struct_size;
    fw_size_f32 size;
    float baseline;
    uint32_t line_count;
    uint32_t did_truncate;
} fw_text_layout_metrics_v1;

typedef struct fw_text_service_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *layout_utf8)(
        void *user_data,
        const fw_text_layout_request_v1 *request,
        fw_text_layout_handle *out_layout,
        fw_text_layout_metrics_v1 *out_metrics);
    void(FW_CALL *release_layout)(
        void *user_data,
        fw_text_layout_handle layout);
} fw_text_service_v1;

#ifdef __cplusplus
}
#endif

#endif
