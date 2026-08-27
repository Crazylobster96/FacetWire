/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_MEDIA_RENDERER_H
#define FACETWIRE_MEDIA_RENDERER_H

#include <facetwire/facetwire.h>
#include <facetwire/geometry.h>
#include <facetwire/renderer.h>
#include <facetwire/render_target.h>
#include <facetwire/semantics.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VIDEO_RENDERER_CAPABILITY_ID "facetwire.renderer.video"
#define FW_VIDEO_RENDERER_INTERFACE_ID "facetwire.renderer.video.v1"
#define FW_AUDIO_RENDERER_CAPABILITY_ID "facetwire.renderer.audio"
#define FW_AUDIO_RENDERER_INTERFACE_ID "facetwire.renderer.audio.v1"
#define FW_MEDIA_RENDERER_INTERFACE_VERSION 1u

typedef uint32_t fw_media_kind;
#define FW_MEDIA_KIND_VIDEO 1u
#define FW_MEDIA_KIND_AUDIO 2u

typedef uint32_t fw_media_output_mode;
#define FW_MEDIA_OUTPUT_EXTERNAL_SURFACE (1u << 0)
#define FW_MEDIA_OUTPUT_DECODED_FRAME    (1u << 1)
#define FW_MEDIA_OUTPUT_POSTER_ONLY      (1u << 2)
#define FW_MEDIA_OUTPUT_ALL (FW_MEDIA_OUTPUT_EXTERNAL_SURFACE | \
    FW_MEDIA_OUTPUT_DECODED_FRAME | FW_MEDIA_OUTPUT_POSTER_ONLY)

typedef uint32_t fw_media_fit;
#define FW_MEDIA_FIT_NONE    0u
#define FW_MEDIA_FIT_CONTAIN 1u
#define FW_MEDIA_FIT_COVER   2u
#define FW_MEDIA_FIT_FILL    3u

typedef uint32_t fw_media_session_state;
#define FW_MEDIA_STATE_IDLE       0u
#define FW_MEDIA_STATE_PREPARING  1u
#define FW_MEDIA_STATE_READY      2u
#define FW_MEDIA_STATE_PLAYING    3u
#define FW_MEDIA_STATE_PAUSED     4u
#define FW_MEDIA_STATE_SEEKING    5u
#define FW_MEDIA_STATE_BUFFERING  6u
#define FW_MEDIA_STATE_ENDED      7u
#define FW_MEDIA_STATE_FAILED     8u

typedef uint32_t fw_media_controls_mode;
#define FW_MEDIA_CONTROLS_AUTO    0u
#define FW_MEDIA_CONTROLS_VISIBLE 1u
#define FW_MEDIA_CONTROLS_HIDDEN  2u

typedef uint32_t fw_media_request_flags;
#define FW_MEDIA_REQUEST_NONE                  0u
#define FW_MEDIA_REQUEST_REDUCE_DATA           (1u << 0)
#define FW_MEDIA_REQUEST_ALLOW_POSTER_FALLBACK (1u << 1)

typedef uint32_t fw_media_normalization_flags;
#define FW_MEDIA_NORMALIZED_NONE                 0u
#define FW_MEDIA_NORMALIZED_AUTOPLAY_SUPPRESSED  (1u << 0)
#define FW_MEDIA_NORMALIZED_OUTPUT_DEGRADED      (1u << 1)

typedef uint64_t fw_media_resource_token;
typedef uint64_t fw_media_frame_token;

typedef struct fw_media_placement_v1 {
    uint32_t struct_size;
    fw_media_fit fit;
    float alignment_x;
    float alignment_y;
    uint32_t clip;
    uint32_t flags;
} fw_media_placement_v1;

typedef struct fw_media_playback_policy_v1 {
    uint32_t struct_size;
    uint32_t autoplay;
    uint32_t loop;
    uint32_t muted;
    float volume;
    float playback_rate;
    uint64_t start_offset_ms;
    uint64_t end_offset_ms;
    uint32_t has_end_offset;
    fw_media_controls_mode controls;
    uint32_t flags;
} fw_media_playback_policy_v1;

typedef struct fw_media_track_v1 {
    uint32_t struct_size;
    fw_string_view resource_id;
    fw_string_view kind;
    fw_string_view language;
    fw_string_view label;
    uint32_t is_default;
    uint32_t flags;
} fw_media_track_v1;

typedef struct fw_media_renderer_request_v1 {
    uint32_t struct_size;
    uint64_t request_id;
    fw_media_kind kind;
    fw_string_view zone_id;
    fw_string_view resource_id;
    fw_string_view label;
    fw_string_view title;
    fw_string_view poster_or_artwork_resource_id;
    float opacity;
    fw_media_placement_v1 placement;
    fw_media_playback_policy_v1 playback;
    const fw_media_track_v1 *tracks;
    size_t track_count;
    fw_layout_constraints_v1 constraints;
    fw_render_target_profile_v1 target;
    uint64_t presentation_revision;
    fw_media_request_flags flags;
} fw_media_renderer_request_v1;

typedef struct fw_media_probe_request_v1 {
    uint32_t struct_size;
    fw_string_view resource_id;
    fw_media_kind kind;
    uint32_t requested_output_modes;
    fw_render_target_profile_v1 target;
    uint32_t flags;
} fw_media_probe_request_v1;

typedef struct fw_media_info_v1 {
    uint32_t struct_size;
    fw_string_view media_type;
    fw_size_f32 intrinsic_visual_size;
    uint64_t duration_ms;
    uint32_t has_duration;
    uint32_t available_output_modes;
    uint32_t has_audio;
    uint32_t has_video;
    uint32_t protected_content;
    uint32_t track_count;
    uint64_t fingerprint_high;
    uint64_t fingerprint_low;
    uint32_t flags;
} fw_media_info_v1;

typedef struct fw_media_open_request_v1 {
    uint32_t struct_size;
    fw_string_view resource_id;
    fw_media_kind kind;
    fw_media_output_mode output_mode;
    uint64_t position_ms;
    fw_render_target_profile_v1 target;
    uint32_t flags;
} fw_media_open_request_v1;

typedef struct fw_media_frame_info_v1 {
    uint32_t struct_size;
    fw_size_f32 visual_size;
    uint64_t timestamp_ms;
    uint64_t duration_ms;
    uint32_t has_alpha;
    uint32_t flags;
} fw_media_frame_info_v1;

typedef struct fw_media_service_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *probe)(void *, const fw_media_probe_request_v1 *,
        fw_media_info_v1 *);
    fw_status(FW_CALL *open)(void *, const fw_media_open_request_v1 *,
        fw_media_resource_token *);
    void(FW_CALL *close)(void *, fw_media_resource_token);
    fw_status(FW_CALL *acquire_frame)(void *, fw_media_resource_token,
        uint64_t, fw_media_frame_token *, fw_media_frame_info_v1 *);
    void(FW_CALL *release_frame)(void *, fw_media_frame_token);
} fw_media_service_v1;

typedef struct fw_media_session_snapshot_v1 {
    uint32_t struct_size;
    uint64_t session_id;
    uint64_t revision;
    fw_media_session_state state;
    uint64_t position_ms;
    uint64_t duration_ms;
    uint64_t buffered_until_ms;
    float playback_rate;
    float effective_volume;
    uint32_t muted;
    fw_string_view selected_track_resource_id;
    uint32_t user_initiated_play;
    uint32_t hidden_from_semantics;
    uint32_t flags;
} fw_media_session_snapshot_v1;

typedef struct fw_media_surface_command_v1 {
    uint32_t struct_size;
    uint64_t session_id;
    fw_string_view zone_id;
    fw_rect_f32 viewport;
    fw_rect_f32 destination;
    fw_rect_f32 source_normalized;
    float opacity;
    uint32_t clip_to_viewport;
    uint32_t show_poster_until_ready;
    uint32_t flags;
} fw_media_surface_command_v1;

typedef struct fw_media_visual_sink_v1 {
    uint32_t struct_size;
    void *user_data;
    fw_status(FW_CALL *place_external_surface)(void *,
        const fw_media_surface_command_v1 *);
    fw_status(FW_CALL *draw_frame)(void *, fw_media_frame_token,
        const fw_media_surface_command_v1 *);
    fw_status(FW_CALL *draw_poster)(void *, fw_string_view,
        const fw_media_surface_command_v1 *);
} fw_media_visual_sink_v1;

typedef struct fw_media_services_v1 {
    uint32_t struct_size;
    const fw_media_service_v1 *media;
    const fw_media_visual_sink_v1 *visual;
    uint32_t flags;
} fw_media_services_v1;

typedef struct fw_media_validation_result_v1 {
    uint32_t struct_size;
    fw_status status;
    fw_media_normalization_flags normalization_flags;
    fw_string_view diagnostic_key;
} fw_media_validation_result_v1;

typedef uint64_t fw_media_semantics_action_mask;
#define FW_MEDIA_ACTION_PLAY          (UINT64_C(1) << 0)
#define FW_MEDIA_ACTION_PAUSE         (UINT64_C(1) << 1)
#define FW_MEDIA_ACTION_SEEK_RELATIVE (UINT64_C(1) << 2)
#define FW_MEDIA_ACTION_SEEK_TO       (UINT64_C(1) << 3)
#define FW_MEDIA_ACTION_SET_RATE      (UINT64_C(1) << 4)
#define FW_MEDIA_ACTION_SET_MUTED     (UINT64_C(1) << 5)
#define FW_MEDIA_ACTION_SET_VOLUME    (UINT64_C(1) << 6)
#define FW_MEDIA_ACTION_SELECT_TRACK  (UINT64_C(1) << 7)

typedef struct fw_media_semantics_v1 {
    uint32_t struct_size;
    fw_semantics_role role;
    fw_string_view label;
    fw_rect_f32 bounds;
    fw_media_session_state state;
    uint64_t position_ms;
    uint64_t duration_ms;
    uint32_t has_duration;
    const fw_media_track_v1 *tracks;
    size_t track_count;
    fw_string_view selected_track_resource_id;
    fw_media_semantics_action_mask actions;
    uint32_t hidden;
    uint32_t flags;
} fw_media_semantics_v1;

typedef struct fw_media_measure_result_v1 {
    uint32_t struct_size;
    fw_size_f32 size;
    fw_size_f32 intrinsic_visual_size;
    fw_media_output_mode selected_output_mode;
    fw_media_normalization_flags normalization_flags;
    uint32_t flags;
} fw_media_measure_result_v1;

typedef struct fw_media_render_result_v1 {
    uint32_t struct_size;
    fw_media_output_mode output_mode;
    fw_rect_f32 destination;
    fw_rect_f32 source_normalized;
    uint32_t command_count;
    uint64_t session_revision;
    uint64_t cache_key_high;
    uint64_t cache_key_low;
    fw_media_normalization_flags normalization_flags;
    uint32_t flags;
} fw_media_render_result_v1;

typedef struct fw_media_renderer_api_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_status(FW_CALL *validate)(fw_plugin_handle,
        const fw_media_renderer_request_v1 *, fw_media_validation_result_v1 *);
    fw_status(FW_CALL *measure)(fw_plugin_handle,
        const fw_media_renderer_request_v1 *, const fw_media_services_v1 *,
        fw_media_measure_result_v1 *);
    fw_status(FW_CALL *render)(fw_plugin_handle,
        const fw_media_renderer_request_v1 *,
        const fw_media_session_snapshot_v1 *, fw_rect_f32,
        const fw_media_services_v1 *, fw_media_render_result_v1 *);
    fw_status(FW_CALL *build_semantics)(fw_plugin_handle,
        const fw_media_renderer_request_v1 *,
        const fw_media_session_snapshot_v1 *, fw_rect_f32,
        fw_media_semantics_v1 *);
    fw_status(FW_CALL *get_parameter_schema)(
        fw_plugin_handle, fw_string_view *out_schema_json);
} fw_media_renderer_api_v1;

#if defined(FACETWIRE_CORE_MEDIA_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_core_media_plugin_query(fw_abi_version requested_abi);

#ifdef __cplusplus
}
#endif

#endif
