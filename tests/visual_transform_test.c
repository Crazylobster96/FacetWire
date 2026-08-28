/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/visual_transform.h>

#include <math.h>
#include <stdio.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", \
        __FILE__, __LINE__, #value); return 1; } } while (0)

static int near(float left, float right) {
    return fabsf(left - right) < 0.001f;
}

int main(void) {
    fw_visual_transform_v1 transform = {
        sizeof(transform), FW_VISUAL_FIT_CONTAIN, 0.5f, 0.5f, 1u,
        FW_VISUAL_ROTATION_0, 0u};
    fw_visual_transform_result_v1 result = {0};
    fw_rect_f32 layer = {0};

    result.struct_size = sizeof(result);
    CHECK(fw_visual_transform_resolve((fw_size_f32){1920.0f, 1080.0f},
        (fw_rect_f32){10.0f, 20.0f, 400.0f, 300.0f}, &transform,
        &result) == FW_STATUS_OK);
    CHECK(near(result.destination.x, 10.0f));
    CHECK(near(result.destination.y, 57.5f));
    CHECK(near(result.destination.width, 400.0f));
    CHECK(near(result.destination.height, 225.0f));
    CHECK(result.uncovered_is_transparent == 1u);

    transform.content_rotation_quarter_turns = FW_VISUAL_ROTATION_90;
    result.struct_size = sizeof(result);
    CHECK(fw_visual_transform_resolve((fw_size_f32){1920.0f, 1080.0f},
        (fw_rect_f32){10.0f, 20.0f, 400.0f, 300.0f}, &transform,
        &result) == FW_STATUS_OK);
    CHECK(near(result.destination.x, 125.625f));
    CHECK(near(result.destination.y, 20.0f));
    CHECK(near(result.destination.width, 168.75f));
    CHECK(near(result.destination.height, 300.0f));
    CHECK(near(result.effective_intrinsic_size.width, 1080.0f));
    CHECK(near(result.effective_intrinsic_size.height, 1920.0f));

    CHECK(fw_visual_transform_layer_bounds(
        (fw_rect_f32){100.0f, 50.0f, 400.0f, 300.0f},
        FW_VISUAL_ROTATION_90, &layer) == FW_STATUS_OK);
    CHECK(near(layer.x, 150.0f) && near(layer.y, 0.0f));
    CHECK(near(layer.width, 300.0f) && near(layer.height, 400.0f));

    transform.content_rotation_quarter_turns = 4u;
    CHECK(fw_visual_transform_validate(&transform) ==
        FW_STATUS_INVALID_ARGUMENT);
    CHECK(fw_visual_transform_layer_bounds(
        (fw_rect_f32){0.0f, 0.0f, 1.0f, 1.0f}, 4u, &layer) ==
        FW_STATUS_INVALID_ARGUMENT);
    puts("visual transform contract passed");
    return 0;
}
