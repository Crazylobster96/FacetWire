/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_GEOMETRY_H
#define FACETWIRE_GEOMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_point_f32 {
    float x;
    float y;
} fw_point_f32;

typedef struct fw_size_f32 {
    float width;
    float height;
} fw_size_f32;

typedef struct fw_rect_f32 {
    float x;
    float y;
    float width;
    float height;
} fw_rect_f32;

typedef struct fw_optional_size_f32 {
    uint32_t has_value;
    fw_size_f32 value;
} fw_optional_size_f32;

typedef struct fw_optional_f32 {
    uint32_t has_value;
    float value;
} fw_optional_f32;

typedef struct fw_layout_constraints_v1 {
    uint32_t struct_size;
    float min_width;
    float max_width;
    float min_height;
    float max_height;
    float em_size;
    float line_height;
    uint32_t flags;
} fw_layout_constraints_v1;

#ifdef __cplusplus
}
#endif

#endif
