/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_DISPLAY_LIST_H
#define FACETWIRE_DISPLAY_LIST_H

#include <facetwire/facetwire.h>
#include <facetwire/geometry.h>
#include <facetwire/render_target.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *fw_text_layout_handle;

typedef struct fw_stroke_style_v1 {
    uint32_t struct_size;
    fw_color_rgba_f32 color;
    float width;
    uint32_t dashed;
} fw_stroke_style_v1;

typedef struct fw_display_list_sink_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *save)(void *user_data);
    fw_status(FW_CALL *restore)(void *user_data);
    fw_status(FW_CALL *clip_rect)(void *user_data, fw_rect_f32 rect);
    fw_status(FW_CALL *fill_rounded_rect)(
        void *user_data,
        fw_rect_f32 rect,
        float radius,
        fw_color_rgba_f32 color);
    fw_status(FW_CALL *stroke_rounded_rect)(
        void *user_data,
        fw_rect_f32 rect,
        float radius,
        const fw_stroke_style_v1 *style);
    fw_status(FW_CALL *draw_symbol)(
        void *user_data,
        fw_string_view symbol_id,
        fw_rect_f32 rect,
        fw_color_rgba_f32 color);
    fw_status(FW_CALL *draw_text_layout)(
        void *user_data,
        fw_text_layout_handle layout,
        fw_point_f32 origin,
        fw_color_rgba_f32 color);
} fw_display_list_sink_v1;

#ifdef __cplusplus
}
#endif

#endif
