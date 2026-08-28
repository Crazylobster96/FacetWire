/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_VISUAL_TRANSFORM_H
#define FACETWIRE_VISUAL_TRANSFORM_H

#include <facetwire/facetwire.h>
#include <facetwire/geometry.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VISUAL_TRANSFORM_VERSION 1u

typedef uint32_t fw_visual_fit;
#define FW_VISUAL_FIT_NONE    0u
#define FW_VISUAL_FIT_CONTAIN 1u
#define FW_VISUAL_FIT_COVER   2u
#define FW_VISUAL_FIT_FILL    3u

typedef uint32_t fw_visual_rotation_quarter_turns;
#define FW_VISUAL_ROTATION_0   0u
#define FW_VISUAL_ROTATION_90  1u
#define FW_VISUAL_ROTATION_180 2u
#define FW_VISUAL_ROTATION_270 3u

typedef struct fw_visual_transform_v1 {
    uint32_t struct_size;
    fw_visual_fit fit;
    float alignment_x;
    float alignment_y;
    uint32_t clip;
    fw_visual_rotation_quarter_turns content_rotation_quarter_turns;
    uint32_t flags;
} fw_visual_transform_v1;

typedef struct fw_visual_transform_result_v1 {
    uint32_t struct_size;
    fw_rect_f32 viewport;
    fw_size_f32 effective_intrinsic_size;
    fw_rect_f32 source_normalized;
    fw_rect_f32 destination;
    fw_visual_rotation_quarter_turns content_rotation_quarter_turns;
    uint32_t clip_to_viewport;
    uint32_t uncovered_is_transparent;
    uint32_t flags;
} fw_visual_transform_result_v1;

FW_API fw_status FW_CALL fw_visual_transform_validate(
    const fw_visual_transform_v1 *transform);

FW_API fw_status FW_CALL fw_visual_transform_resolve(
    fw_size_f32 intrinsic_size,
    fw_rect_f32 viewport,
    const fw_visual_transform_v1 *transform,
    fw_visual_transform_result_v1 *out_result);

FW_API fw_status FW_CALL fw_visual_transform_layer_bounds(
    fw_rect_f32 bounds,
    fw_visual_rotation_quarter_turns layer_rotation_quarter_turns,
    fw_rect_f32 *out_bounds);

#ifdef __cplusplus
}
#endif

#endif
