/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_CHILD_MEASURE_SERVICE_H
#define FACETWIRE_CHILD_MEASURE_SERVICE_H

#include <facetwire/flow_layout.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_child_measure_request_v1 {
    uint32_t struct_size;
    fw_string_view item_id;
    fw_string_view content_id;
    fw_string_view content_kind;
    fw_layout_constraints_v1 constraints;
    fw_render_target_profile_v1 target;
    uint32_t flags;
} fw_child_measure_request_v1;

typedef struct fw_child_measure_result_v1 {
    uint32_t struct_size;
    fw_size_f32 intrinsic_size;
    fw_optional_f32 aspect_ratio;
    fw_size_f32 fallback_size;
    uint32_t has_intrinsic_size;
    uint32_t splittable;
    uint32_t used_fallback;
    uint64_t fingerprint_high;
    uint64_t fingerprint_low;
    uint32_t flags;
} fw_child_measure_result_v1;

typedef struct fw_child_measure_service_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *measure_child)(void *user_data,
        const fw_child_measure_request_v1 *request,
        fw_child_measure_result_v1 *out_result);
} fw_child_measure_service_v1;

#ifdef __cplusplus
}
#endif

#endif
