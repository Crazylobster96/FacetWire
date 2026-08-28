/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/visual_transform.h>

#include <math.h>
#include <string.h>

static int vt_valid_rect(fw_rect_f32 value) {
    return isfinite(value.x) && isfinite(value.y) &&
        isfinite(value.width) && value.width >= 0.0f &&
        isfinite(value.height) && value.height >= 0.0f;
}

static int vt_valid_size(fw_size_f32 value) {
    return isfinite(value.width) && value.width >= 0.0f &&
        isfinite(value.height) && value.height >= 0.0f;
}

fw_status FW_CALL fw_visual_transform_validate(
    const fw_visual_transform_v1 *transform) {
    if (transform == NULL || transform->struct_size < sizeof(*transform) ||
        transform->fit > FW_VISUAL_FIT_FILL ||
        !isfinite(transform->alignment_x) ||
        transform->alignment_x < 0.0f || transform->alignment_x > 1.0f ||
        !isfinite(transform->alignment_y) ||
        transform->alignment_y < 0.0f || transform->alignment_y > 1.0f ||
        transform->clip > 1u ||
        transform->content_rotation_quarter_turns >
            FW_VISUAL_ROTATION_270)
        return FW_STATUS_INVALID_ARGUMENT;
    return FW_STATUS_OK;
}

fw_status FW_CALL fw_visual_transform_resolve(
    fw_size_f32 intrinsic_size,
    fw_rect_f32 viewport,
    const fw_visual_transform_v1 *transform,
    fw_visual_transform_result_v1 *out_result) {
    fw_size_f32 effective;
    float width;
    float height;
    float sx;
    float sy;
    float scale;
    uint32_t result_size;
    fw_status status;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        !vt_valid_size(intrinsic_size) || !vt_valid_rect(viewport))
        return FW_STATUS_INVALID_ARGUMENT;
    status = fw_visual_transform_validate(transform);
    if (status != FW_STATUS_OK) return status;
    result_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = result_size;
    out_result->viewport = viewport;
    out_result->source_normalized =
        (fw_rect_f32){0.0f, 0.0f, 1.0f, 1.0f};
    out_result->content_rotation_quarter_turns =
        transform->content_rotation_quarter_turns;
    out_result->clip_to_viewport = transform->clip;
    out_result->uncovered_is_transparent = 1u;
    effective = intrinsic_size;
    if (effective.width > 0.0f && effective.height > 0.0f &&
        (transform->content_rotation_quarter_turns & 1u) != 0u) {
        const float original_width = effective.width;
        effective.width = effective.height;
        effective.height = original_width;
    }
    if (effective.width <= 0.0f || effective.height <= 0.0f)
        effective = (fw_size_f32){viewport.width, viewport.height};
    out_result->effective_intrinsic_size = effective;
    if (viewport.width == 0.0f || viewport.height == 0.0f ||
        effective.width == 0.0f || effective.height == 0.0f) {
        out_result->destination =
            (fw_rect_f32){viewport.x, viewport.y, 0.0f, 0.0f};
        return FW_STATUS_OK;
    }
    width = effective.width;
    height = effective.height;
    sx = viewport.width / effective.width;
    sy = viewport.height / effective.height;
    if (transform->fit == FW_VISUAL_FIT_FILL) {
        width = viewport.width;
        height = viewport.height;
    } else if (transform->fit == FW_VISUAL_FIT_CONTAIN ||
        transform->fit == FW_VISUAL_FIT_COVER) {
        scale = transform->fit == FW_VISUAL_FIT_CONTAIN ?
            (sx < sy ? sx : sy) : (sx > sy ? sx : sy);
        width = effective.width * scale;
        height = effective.height * scale;
    }
    out_result->destination.x = viewport.x +
        (viewport.width - width) * transform->alignment_x;
    out_result->destination.y = viewport.y +
        (viewport.height - height) * transform->alignment_y;
    out_result->destination.width = width;
    out_result->destination.height = height;
    return FW_STATUS_OK;
}

fw_status FW_CALL fw_visual_transform_layer_bounds(
    fw_rect_f32 bounds,
    fw_visual_rotation_quarter_turns layer_rotation_quarter_turns,
    fw_rect_f32 *out_bounds) {
    if (out_bounds == NULL || !vt_valid_rect(bounds) ||
        layer_rotation_quarter_turns > FW_VISUAL_ROTATION_270)
        return FW_STATUS_INVALID_ARGUMENT;
    *out_bounds = bounds;
    if ((layer_rotation_quarter_turns & 1u) != 0u) {
        const float center_x = bounds.x + bounds.width * 0.5f;
        const float center_y = bounds.y + bounds.height * 0.5f;
        out_bounds->width = bounds.height;
        out_bounds->height = bounds.width;
        out_bounds->x = center_x - out_bounds->width * 0.5f;
        out_bounds->y = center_y - out_bounds->height * 0.5f;
    }
    return FW_STATUS_OK;
}
