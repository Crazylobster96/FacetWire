/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_RENDER_TARGET_H
#define FACETWIRE_RENDER_TARGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_color_rgba_f32 {
    float red;
    float green;
    float blue;
    float alpha;
} fw_color_rgba_f32;

typedef uint32_t fw_render_medium;
#define FW_RENDER_MEDIUM_SCREEN   1u
#define FW_RENDER_MEDIUM_PRINT    2u
#define FW_RENDER_MEDIUM_EXPORT   3u
#define FW_RENDER_MEDIUM_HEADLESS 4u

typedef struct fw_render_target_profile_v1 {
    uint32_t struct_size;
    float device_pixel_ratio;
    float font_scale;
    fw_render_medium medium;
    uint32_t prefers_dark;
    uint32_t high_contrast;
    uint32_t reduce_motion;
    uint32_t supports_alpha;
    uint32_t flags;
} fw_render_target_profile_v1;

#ifdef __cplusplus
}
#endif

#endif
