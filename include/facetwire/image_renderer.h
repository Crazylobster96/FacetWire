/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_IMAGE_RENDERER_H
#define FACETWIRE_IMAGE_RENDERER_H

#include <facetwire/facetwire.h>
#include <facetwire/geometry.h>
#include <facetwire/renderer.h>
#include <facetwire/render_target.h>
#include <facetwire/semantics.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_IMAGE_RENDERER_CAPABILITY_ID "facetwire.renderer.image"
#define FW_IMAGE_RENDERER_INTERFACE_ID "facetwire.renderer.image.v1"
#define FW_ANIMATED_IMAGE_RENDERER_CAPABILITY_ID \
    "facetwire.renderer.animated-image"
#define FW_ANIMATED_IMAGE_RENDERER_INTERFACE_ID \
    "facetwire.renderer.animated-image.v1"
#define FW_IMAGE_RENDERER_INTERFACE_VERSION 1u

typedef void *fw_image_handle;

typedef uint32_t fw_image_content_kind;
#define FW_IMAGE_CONTENT_STATIC   1u
#define FW_IMAGE_CONTENT_ANIMATED 2u

typedef uint32_t fw_image_fit;
#define FW_IMAGE_FIT_NONE    0u
#define FW_IMAGE_FIT_CONTAIN 1u
#define FW_IMAGE_FIT_COVER   2u
#define FW_IMAGE_FIT_FILL    3u

typedef uint32_t fw_image_sampling;
#define FW_IMAGE_SAMPLING_AUTO       0u
#define FW_IMAGE_SAMPLING_SMOOTH     1u
#define FW_IMAGE_SAMPLING_PIXELATED  2u

typedef struct fw_image_placement_v1 {
    uint32_t struct_size;
    fw_image_fit fit;
    float alignment_x;
    float alignment_y;
    uint32_t clip;
    fw_image_sampling sampling;
    uint32_t flags;
} fw_image_placement_v1;

typedef struct fw_image_playback_v1 {
    uint32_t struct_size;
    uint32_t autoplay;
    uint32_t loop;
    float playback_rate;
    uint64_t position_ms;
    uint32_t playing;
    uint32_t flags;
} fw_image_playback_v1;

typedef struct fw_image_renderer_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_string_view zone_id;
    fw_string_view resource_id;
    fw_string_view alt;
    fw_image_content_kind kind;
    float opacity;
    fw_image_placement_v1 placement;
    fw_image_playback_v1 playback;
    fw_layout_constraints_v1 constraints;
    fw_render_target_profile_v1 target;
    uint64_t presentation_revision;
    uint32_t flags;
} fw_image_renderer_request_v1;

typedef struct fw_image_acquire_request_v1 {
    uint32_t struct_size;
    fw_string_view resource_id;
    fw_image_content_kind kind;
    uint64_t position_ms;
    uint32_t reduce_motion;
    uint32_t flags;
} fw_image_acquire_request_v1;

typedef struct fw_image_info_v1 {
    uint32_t struct_size;
    fw_size_f32 intrinsic_size;
    uint32_t frame_count;
    uint32_t frame_index;
    uint64_t duration_ms;
    uint32_t has_alpha;
    fw_string_view media_type;
    uint64_t fingerprint_high;
    uint64_t fingerprint_low;
    uint32_t flags;
} fw_image_info_v1;

typedef struct fw_image_service_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *acquire)(void *, const fw_image_acquire_request_v1 *,
        fw_image_handle *, fw_image_info_v1 *);
    void(FW_CALL *release)(void *, fw_image_handle);
} fw_image_service_v1;

typedef struct fw_image_draw_sink_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *save)(void *);
    fw_status(FW_CALL *restore)(void *);
    fw_status(FW_CALL *clip_rect)(void *, fw_rect_f32);
    fw_status(FW_CALL *draw_image)(void *, fw_image_handle,
        fw_rect_f32 source, fw_rect_f32 destination,
        float opacity, fw_image_sampling sampling);
} fw_image_draw_sink_v1;

typedef struct fw_image_services_v1 {
    uint32_t struct_size;
    const fw_image_service_v1 *images;
    const fw_image_draw_sink_v1 *draw;
    uint32_t flags;
} fw_image_services_v1;

typedef struct fw_image_validation_result_v1 {
    uint32_t struct_size;
    fw_status status;
    uint32_t normalization_flags;
    fw_string_view diagnostic_key;
} fw_image_validation_result_v1;

typedef struct fw_image_measure_result_v1 {
    uint32_t struct_size;
    fw_size_f32 size;
    fw_size_f32 intrinsic_size;
    uint32_t used_fallback;
    uint32_t flags;
} fw_image_measure_result_v1;

typedef struct fw_image_render_result_v1 {
    uint32_t struct_size;
    fw_rect_f32 source_rect;
    fw_rect_f32 destination_rect;
    uint32_t frame_index;
    uint32_t frame_count;
    uint32_t emitted_command_count;
    uint64_t cache_key_high;
    uint64_t cache_key_low;
    uint32_t flags;
} fw_image_render_result_v1;

typedef struct fw_image_semantics_v1 {
    uint32_t struct_size;
    uint32_t role;
    fw_string_view label;
    fw_rect_f32 bounds;
    uint32_t decorative;
    uint32_t animated;
    uint32_t flags;
} fw_image_semantics_v1;

typedef struct fw_image_renderer_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_image_renderer_request_v1 *, fw_image_validation_result_v1 *);
    fw_status(FW_CALL *measure)(fw_plugin_handle,
        const fw_image_renderer_request_v1 *, const fw_image_services_v1 *,
        fw_image_measure_result_v1 *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_image_renderer_request_v1 *, fw_rect_f32,
        const fw_image_services_v1 *, fw_image_render_result_v1 *);
    fw_status(FW_CALL *build_semantics)(fw_plugin_handle,
        const fw_image_renderer_request_v1 *, fw_rect_f32,
        fw_image_semantics_v1 *);
    fw_status(FW_CALL *get_parameter_schema)(
        fw_plugin_handle, fw_string_view *out_schema_json);
} fw_image_renderer_api_v1;

#if defined(FACETWIRE_CORE_IMAGE_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_core_image_plugin_query(fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
