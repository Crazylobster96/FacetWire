/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/media_renderer.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MR_MAGIC UINT32_C(0x4d445231)
#define MR_MAX_STRING_BYTES 8192u
#define MR_MAX_TRACKS 1024u

typedef struct mr_context {
    uint32_t magic;
    fw_host_api_v1 host;
} mr_context;

static const fw_capability_descriptor_v1 mr_capabilities[] = {
    {sizeof(fw_capability_descriptor_v1),
     FW_STRING_VIEW_LITERAL(FW_VIDEO_RENDERER_CAPABILITY_ID),
     FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
     FW_RENDERER_FLAG_HEADLESS | FW_RENDERER_FLAG_SEMANTICS},
    {sizeof(fw_capability_descriptor_v1),
     FW_STRING_VIEW_LITERAL(FW_AUDIO_RENDERER_CAPABILITY_ID),
     FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
     FW_RENDERER_FLAG_HEADLESS | FW_RENDERER_FLAG_SEMANTICS},
};

static const fw_plugin_descriptor_v1 mr_descriptor = {
    sizeof(fw_plugin_descriptor_v1), FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.core-media-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Core Media Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    mr_capabilities, sizeof(mr_capabilities) / sizeof(mr_capabilities[0]),
};

static const char mr_parameter_schema[] =
    "{\"schemaVersion\":1,\"parameters\":["
    "{\"id\":\"opacity\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0,\"maximum\":1,\"step\":0.01},"
    "{\"id\":\"fit\",\"type\":\"enum\",\"default\":\"contain\","
    "\"values\":[\"none\",\"contain\",\"cover\",\"fill\"]},"
    "{\"id\":\"volume\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0,\"maximum\":1,\"step\":0.01},"
    "{\"id\":\"playbackRate\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0.05,\"maximum\":16}]}";

static int mr_context_valid(fw_plugin_handle plugin) {
    const mr_context *context = (const mr_context *)plugin;
    return context != NULL && context->magic == MR_MAGIC;
}

static fw_string_view mr_view(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static int mr_string_shape(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int mr_string_equal(fw_string_view value, const char *literal) {
    const size_t length = strlen(literal);
    return value.length == length &&
        (length == 0u || memcmp(value.data, literal, length) == 0);
}

static int mr_utf8_valid(fw_string_view value) {
    size_t i = 0u;
    while (i < value.length) {
        const unsigned char first = (unsigned char)value.data[i++];
        size_t count;
        uint32_t cp;
        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            count = 1u; cp = first & 0x1fu;
        } else if (first >= 0xe0u && first <= 0xefu) {
            count = 2u; cp = first & 0x0fu;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            count = 3u; cp = first & 0x07u;
        } else return 0;
        if (count > value.length - i) return 0;
        while (count-- != 0u) {
            const unsigned char next = (unsigned char)value.data[i++];
            if ((next & 0xc0u) != 0x80u) return 0;
            cp = (cp << 6u) | (next & 0x3fu);
        }
        if ((cp >= 0xd800u && cp <= 0xdfffu) || cp > 0x10ffffu ||
            (cp < 0x800u && first >= 0xe0u) ||
            (cp < 0x10000u && first >= 0xf0u)) return 0;
    }
    return 1;
}

static int mr_valid_string(fw_string_view value, int required) {
    return mr_string_shape(value) &&
        (!required || value.length != 0u) &&
        value.length <= MR_MAX_STRING_BYTES && mr_utf8_valid(value);
}

static int mr_finite_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
}

static int mr_track_kind_valid(fw_string_view kind) {
    return mr_string_equal(kind, "subtitles") ||
        mr_string_equal(kind, "captions") ||
        mr_string_equal(kind, "descriptions") ||
        mr_string_equal(kind, "chapters") ||
        mr_string_equal(kind, "metadata");
}

static fw_media_normalization_flags mr_normalization(
    const fw_media_renderer_request_v1 *request) {
    fw_media_normalization_flags flags = FW_MEDIA_NORMALIZED_NONE;
    if (request->target.reduce_motion != 0u &&
        request->playback.autoplay != 0u)
        flags |= FW_MEDIA_NORMALIZED_AUTOPLAY_SUPPRESSED;
    return flags;
}

static fw_status mr_validate_request(fw_plugin_handle plugin,
    const fw_media_renderer_request_v1 *request,
    fw_media_normalization_flags *out_flags, const char **out_key) {
    size_t i;
    uint32_t default_tracks = 0u;
    *out_flags = FW_MEDIA_NORMALIZED_NONE;
    *out_key = "media.invalid_argument";
    if (!mr_context_valid(plugin) || request == NULL ||
        request->struct_size < sizeof(*request) ||
        request->placement.struct_size < sizeof(request->placement) ||
        request->playback.struct_size < sizeof(request->playback) ||
        request->constraints.struct_size < sizeof(request->constraints) ||
        request->target.struct_size < sizeof(request->target)) {
        *out_key = "media.invalid_struct_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!mr_valid_string(request->zone_id, 0) ||
        !mr_valid_string(request->resource_id, 1) ||
        !mr_valid_string(request->label, 1) ||
        !mr_valid_string(request->title, 0) ||
        !mr_valid_string(request->poster_or_artwork_resource_id, 0)) {
        *out_key = "media.invalid_string";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->kind != FW_MEDIA_KIND_VIDEO &&
        request->kind != FW_MEDIA_KIND_AUDIO) {
        *out_key = "media.invalid_kind";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->placement.fit > FW_MEDIA_FIT_FILL ||
        request->placement.clip > 1u ||
        request->playback.autoplay > 1u ||
        request->playback.loop > 1u || request->playback.muted > 1u ||
        request->playback.has_end_offset > 1u ||
        request->playback.controls > FW_MEDIA_CONTROLS_HIDDEN ||
        request->target.medium < FW_RENDER_MEDIUM_SCREEN ||
        request->target.medium > FW_RENDER_MEDIUM_HEADLESS ||
        request->target.prefers_dark > 1u ||
        request->target.high_contrast > 1u ||
        request->target.reduce_motion > 1u ||
        request->target.supports_alpha > 1u) {
        *out_key = "media.invalid_enum";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->opacity) || request->opacity < 0.0f ||
        request->opacity > 1.0f ||
        !isfinite(request->placement.alignment_x) ||
        request->placement.alignment_x < 0.0f ||
        request->placement.alignment_x > 1.0f ||
        !isfinite(request->placement.alignment_y) ||
        request->placement.alignment_y < 0.0f ||
        request->placement.alignment_y > 1.0f ||
        !isfinite(request->playback.volume) ||
        request->playback.volume < 0.0f ||
        request->playback.volume > 1.0f ||
        !isfinite(request->playback.playback_rate) ||
        request->playback.playback_rate <= 0.0f ||
        request->playback.playback_rate > 16.0f ||
        !mr_finite_nonnegative(request->constraints.min_width) ||
        !mr_finite_nonnegative(request->constraints.max_width) ||
        !mr_finite_nonnegative(request->constraints.min_height) ||
        !mr_finite_nonnegative(request->constraints.max_height) ||
        request->constraints.max_width < request->constraints.min_width ||
        request->constraints.max_height < request->constraints.min_height ||
        !isfinite(request->target.device_pixel_ratio) ||
        request->target.device_pixel_ratio <= 0.0f ||
        !isfinite(request->target.font_scale) ||
        request->target.font_scale <= 0.0f) {
        *out_key = "media.invalid_geometry";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->playback.has_end_offset != 0u &&
        request->playback.end_offset_ms <= request->playback.start_offset_ms) {
        *out_key = "media.invalid_time_range";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->track_count > MR_MAX_TRACKS ||
        (request->track_count != 0u && request->tracks == NULL)) {
        *out_key = "media.invalid_tracks";
        return request->track_count > MR_MAX_TRACKS ?
            FW_STATUS_RESOURCE_LIMIT : FW_STATUS_INVALID_ARGUMENT;
    }
    for (i = 0u; i < request->track_count; ++i) {
        const fw_media_track_v1 *track = &request->tracks[i];
        if (track->struct_size < sizeof(*track) ||
            !mr_valid_string(track->resource_id, 1) ||
            !mr_valid_string(track->kind, 1) ||
            !mr_track_kind_valid(track->kind) ||
            !mr_valid_string(track->language, 0) ||
            !mr_valid_string(track->label, 0) || track->is_default > 1u) {
            *out_key = "media.invalid_track";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        default_tracks += track->is_default;
        if (default_tracks > 1u) {
            *out_key = "media.multiple_default_tracks";
            return FW_STATUS_INVALID_ARGUMENT;
        }
    }
    *out_flags = mr_normalization(request);
    *out_key = "media.valid";
    return FW_STATUS_OK;
}

static fw_status mr_validate_snapshot(
    const fw_media_renderer_request_v1 *request,
    const fw_media_session_snapshot_v1 *snapshot) {
    if (snapshot == NULL || snapshot->struct_size < sizeof(*snapshot) ||
        snapshot->state > FW_MEDIA_STATE_FAILED ||
        snapshot->revision < request->presentation_revision ||
        !isfinite(snapshot->playback_rate) || snapshot->playback_rate <= 0.0f ||
        !isfinite(snapshot->effective_volume) ||
        snapshot->effective_volume < 0.0f ||
        snapshot->effective_volume > 1.0f || snapshot->muted > 1u ||
        snapshot->user_initiated_play > 1u ||
        snapshot->hidden_from_semantics > 1u ||
        !mr_valid_string(snapshot->selected_track_resource_id, 0))
        return snapshot != NULL &&
            snapshot->revision < request->presentation_revision ?
            FW_STATUS_INVALID_STATE : FW_STATUS_INVALID_ARGUMENT;
    return FW_STATUS_OK;
}

static fw_status mr_probe(const fw_media_renderer_request_v1 *request,
    const fw_media_services_v1 *services, fw_media_info_v1 *out_info) {
    fw_media_probe_request_v1 probe;
    fw_status status;
    const fw_media_service_v1 *media;
    if (services == NULL || services->struct_size < sizeof(*services) ||
        services->media == NULL) return FW_STATUS_INVALID_ARGUMENT;
    media = services->media;
    if (media->struct_size < sizeof(*media) || media->probe == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    memset(&probe, 0, sizeof(probe));
    probe.struct_size = sizeof(probe);
    probe.resource_id = request->resource_id;
    probe.kind = request->kind;
    probe.requested_output_modes = FW_MEDIA_OUTPUT_ALL;
    probe.target = request->target;
    probe.flags = request->flags;
    memset(out_info, 0, sizeof(*out_info));
    out_info->struct_size = sizeof(*out_info);
    status = media->probe(media->user_data, &probe, out_info);
    if (status != FW_STATUS_OK) return status;
    if (out_info->struct_size < sizeof(*out_info) ||
        !mr_valid_string(out_info->media_type, 0) ||
        !mr_finite_nonnegative(out_info->intrinsic_visual_size.width) ||
        !mr_finite_nonnegative(out_info->intrinsic_visual_size.height) ||
        (out_info->available_output_modes & ~FW_MEDIA_OUTPUT_ALL) != 0u ||
        out_info->has_duration > 1u || out_info->has_audio > 1u ||
        out_info->has_video > 1u || out_info->protected_content > 1u)
        return FW_STATUS_PLUGIN_ERROR;
    if ((request->kind == FW_MEDIA_KIND_VIDEO && out_info->has_video == 0u) ||
        (request->kind == FW_MEDIA_KIND_AUDIO && out_info->has_audio == 0u))
        return FW_STATUS_UNSUPPORTED;
    return FW_STATUS_OK;
}

static fw_status mr_select_output(const fw_media_renderer_request_v1 *request,
    const fw_media_info_v1 *info, fw_media_output_mode *out_mode,
    fw_media_normalization_flags *inout_flags) {
    const uint32_t available = info->available_output_modes;
    const int has_poster =
        request->poster_or_artwork_resource_id.length != 0u;
    const int static_target = request->target.medium != FW_RENDER_MEDIUM_SCREEN ||
        (request->flags & FW_MEDIA_REQUEST_REDUCE_DATA) != 0u;
    *out_mode = 0u;
    if (request->kind == FW_MEDIA_KIND_AUDIO) {
        if (has_poster &&
            (available & FW_MEDIA_OUTPUT_POSTER_ONLY) != 0u) {
            *out_mode = FW_MEDIA_OUTPUT_POSTER_ONLY;
            return FW_STATUS_OK;
        }
        if (!static_target &&
            (available & FW_MEDIA_OUTPUT_EXTERNAL_SURFACE) != 0u) {
            *out_mode = FW_MEDIA_OUTPUT_EXTERNAL_SURFACE;
            return FW_STATUS_OK;
        }
        return FW_STATUS_UNSUPPORTED;
    }
    if (static_target) {
        if (has_poster && (available & FW_MEDIA_OUTPUT_POSTER_ONLY) != 0u) {
            *out_mode = FW_MEDIA_OUTPUT_POSTER_ONLY;
            return FW_STATUS_OK;
        }
        if (!info->protected_content &&
            (available & FW_MEDIA_OUTPUT_DECODED_FRAME) != 0u) {
            *out_mode = FW_MEDIA_OUTPUT_DECODED_FRAME;
            return FW_STATUS_OK;
        }
        return FW_STATUS_UNSUPPORTED;
    }
    if ((available & FW_MEDIA_OUTPUT_EXTERNAL_SURFACE) != 0u) {
        *out_mode = FW_MEDIA_OUTPUT_EXTERNAL_SURFACE;
        return FW_STATUS_OK;
    }
    if (!info->protected_content &&
        (available & FW_MEDIA_OUTPUT_DECODED_FRAME) != 0u) {
        *out_mode = FW_MEDIA_OUTPUT_DECODED_FRAME;
        return FW_STATUS_OK;
    }
    if (has_poster && (available & FW_MEDIA_OUTPUT_POSTER_ONLY) != 0u &&
        (request->flags & FW_MEDIA_REQUEST_ALLOW_POSTER_FALLBACK) != 0u) {
        *out_mode = FW_MEDIA_OUTPUT_POSTER_ONLY;
        *inout_flags |= FW_MEDIA_NORMALIZED_OUTPUT_DEGRADED;
        return FW_STATUS_OK;
    }
    return FW_STATUS_UNSUPPORTED;
}

static float mr_clamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static fw_size_f32 mr_measure_size(fw_size_f32 intrinsic,
    const fw_layout_constraints_v1 *constraints) {
    fw_size_f32 size = intrinsic;
    float scale = 1.0f;
    if (size.width <= 0.0f || size.height <= 0.0f) {
        size.width = constraints->min_width;
        size.height = constraints->min_height;
        return size;
    }
    if (constraints->max_width > 0.0f && size.width > constraints->max_width)
        scale = constraints->max_width / size.width;
    if (constraints->max_height > 0.0f && size.height * scale >
        constraints->max_height)
        scale = constraints->max_height / size.height;
    size.width *= scale;
    size.height *= scale;
    size.width = mr_clamp(size.width, constraints->min_width,
        constraints->max_width > 0.0f ? constraints->max_width : size.width);
    size.height = mr_clamp(size.height, constraints->min_height,
        constraints->max_height > 0.0f ? constraints->max_height : size.height);
    return size;
}

static void mr_placement(fw_size_f32 intrinsic, fw_rect_f32 bounds,
    const fw_media_placement_v1 *placement, fw_rect_f32 *out_source,
    fw_rect_f32 *out_destination) {
    float width;
    float height;
    float sx;
    float sy;
    float scale;
    *out_source = (fw_rect_f32){0.0f, 0.0f, 1.0f, 1.0f};
    if (intrinsic.width <= 0.0f || intrinsic.height <= 0.0f) {
        intrinsic.width = bounds.width;
        intrinsic.height = bounds.height;
    }
    width = intrinsic.width;
    height = intrinsic.height;
    sx = intrinsic.width > 0.0f ? bounds.width / intrinsic.width : 1.0f;
    sy = intrinsic.height > 0.0f ? bounds.height / intrinsic.height : 1.0f;
    if (placement->fit == FW_MEDIA_FIT_FILL) {
        width = bounds.width;
        height = bounds.height;
    } else if (placement->fit == FW_MEDIA_FIT_CONTAIN ||
        placement->fit == FW_MEDIA_FIT_COVER) {
        scale = placement->fit == FW_MEDIA_FIT_CONTAIN ?
            (sx < sy ? sx : sy) : (sx > sy ? sx : sy);
        width = intrinsic.width * scale;
        height = intrinsic.height * scale;
    }
    out_destination->x = bounds.x +
        (bounds.width - width) * placement->alignment_x;
    out_destination->y = bounds.y +
        (bounds.height - height) * placement->alignment_y;
    out_destination->width = width;
    out_destination->height = height;
}

static void mr_hash_bytes(uint64_t *high, uint64_t *low,
    const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0u; i < length; ++i) {
        *low ^= bytes[i]; *low *= UINT64_C(1099511628211);
        *high ^= (*low >> 29u) ^ bytes[i];
        *high *= UINT64_C(14029467366897019727);
    }
}

static void mr_hash_view(uint64_t *high, uint64_t *low, fw_string_view view) {
    mr_hash_bytes(high, low, &view.length, sizeof(view.length));
    mr_hash_bytes(high, low, view.data, view.length);
}

static void mr_cache_key(const fw_media_renderer_request_v1 *request,
    const fw_media_session_snapshot_v1 *snapshot,
    const fw_media_info_v1 *info, fw_media_output_mode mode,
    fw_rect_f32 destination, fw_rect_f32 source,
    uint64_t *out_high, uint64_t *out_low) {
    size_t i;
    uint64_t high = UINT64_C(7809847782465536322);
    uint64_t low = UINT64_C(1469598103934665603);
    mr_hash_view(&high, &low, request->resource_id);
    mr_hash_view(&high, &low, request->poster_or_artwork_resource_id);
    mr_hash_bytes(&high, &low, &request->kind, sizeof(request->kind));
    mr_hash_bytes(&high, &low, &request->opacity, sizeof(request->opacity));
    mr_hash_bytes(&high, &low, &request->placement.fit,
        sizeof(request->placement.fit));
    mr_hash_bytes(&high, &low, &request->placement.alignment_x,
        sizeof(request->placement.alignment_x));
    mr_hash_bytes(&high, &low, &request->placement.alignment_y,
        sizeof(request->placement.alignment_y));
    mr_hash_bytes(&high, &low, &request->placement.clip,
        sizeof(request->placement.clip));
    mr_hash_bytes(&high, &low, &request->target.medium,
        sizeof(request->target.medium));
    mr_hash_bytes(&high, &low, &request->target.device_pixel_ratio,
        sizeof(request->target.device_pixel_ratio));
    mr_hash_bytes(&high, &low, &mode, sizeof(mode));
    mr_hash_bytes(&high, &low, &destination, sizeof(destination));
    mr_hash_bytes(&high, &low, &source, sizeof(source));
    mr_hash_bytes(&high, &low, &info->fingerprint_high,
        sizeof(info->fingerprint_high));
    mr_hash_bytes(&high, &low, &info->fingerprint_low,
        sizeof(info->fingerprint_low));
    mr_hash_bytes(&high, &low, &snapshot->revision,
        sizeof(snapshot->revision));
    mr_hash_bytes(&high, &low, &snapshot->state, sizeof(snapshot->state));
    for (i = 0u; i < request->track_count; ++i) {
        mr_hash_view(&high, &low, request->tracks[i].resource_id);
        mr_hash_view(&high, &low, request->tracks[i].kind);
        mr_hash_view(&high, &low, request->tracks[i].language);
    }
    *out_high = high;
    *out_low = low;
}

static fw_status FW_CALL mr_validate(fw_plugin_handle plugin,
    const fw_media_renderer_request_v1 *request,
    fw_media_validation_result_v1 *out_result) {
    fw_media_normalization_flags flags;
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = mr_validate_request(plugin, request, &flags, &key);
    out_result->status = status;
    out_result->normalization_flags = flags;
    out_result->diagnostic_key = mr_view(key);
    return status;
}

static fw_status FW_CALL mr_measure(fw_plugin_handle plugin,
    const fw_media_renderer_request_v1 *request,
    const fw_media_services_v1 *services,
    fw_media_measure_result_v1 *out_result) {
    fw_media_normalization_flags flags;
    fw_media_output_mode mode;
    fw_media_info_v1 info;
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = mr_validate_request(plugin, request, &flags, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    status = mr_probe(request, services, &info);
    if (status != FW_STATUS_OK) return status;
    status = mr_select_output(request, &info, &mode, &flags);
    if (status != FW_STATUS_OK) return status;
    out_result->intrinsic_visual_size = info.intrinsic_visual_size;
    out_result->size = mr_measure_size(info.intrinsic_visual_size,
        &request->constraints);
    out_result->selected_output_mode = mode;
    out_result->normalization_flags = flags;
    return FW_STATUS_OK;
}

static fw_status mr_draw_decoded(
    const fw_media_renderer_request_v1 *request,
    const fw_media_session_snapshot_v1 *snapshot,
    const fw_media_services_v1 *services,
    const fw_media_surface_command_v1 *command) {
    fw_media_open_request_v1 open_request;
    fw_media_frame_info_v1 frame_info;
    fw_media_resource_token resource = 0u;
    fw_media_frame_token frame = 0u;
    fw_status status;
    uint64_t position = snapshot->position_ms;
    const fw_media_service_v1 *media = services->media;
    const fw_media_visual_sink_v1 *visual = services->visual;
    if (media->open == NULL || media->close == NULL ||
        media->acquire_frame == NULL || media->release_frame == NULL ||
        visual == NULL || visual->struct_size < sizeof(*visual) ||
        visual->draw_frame == NULL) return FW_STATUS_INVALID_ARGUMENT;
    if (position < request->playback.start_offset_ms)
        position = request->playback.start_offset_ms;
    if (request->playback.has_end_offset != 0u &&
        position > request->playback.end_offset_ms)
        position = request->playback.end_offset_ms;
    memset(&open_request, 0, sizeof(open_request));
    open_request.struct_size = sizeof(open_request);
    open_request.resource_id = request->resource_id;
    open_request.kind = request->kind;
    open_request.output_mode = FW_MEDIA_OUTPUT_DECODED_FRAME;
    open_request.position_ms = position;
    open_request.target = request->target;
    open_request.flags = request->flags;
    status = media->open(media->user_data, &open_request, &resource);
    if (status != FW_STATUS_OK || resource == 0u) {
        if (resource != 0u) media->close(media->user_data, resource);
        return status == FW_STATUS_OK ? FW_STATUS_PLUGIN_ERROR : status;
    }
    memset(&frame_info, 0, sizeof(frame_info));
    frame_info.struct_size = sizeof(frame_info);
    status = media->acquire_frame(media->user_data, resource, position,
        &frame, &frame_info);
    if (status != FW_STATUS_OK || frame == 0u ||
        frame_info.struct_size < sizeof(frame_info) ||
        !mr_finite_nonnegative(frame_info.visual_size.width) ||
        !mr_finite_nonnegative(frame_info.visual_size.height)) {
        if (frame != 0u) media->release_frame(media->user_data, frame);
        media->close(media->user_data, resource);
        return status == FW_STATUS_OK ? FW_STATUS_PLUGIN_ERROR : status;
    }
    status = visual->draw_frame(visual->user_data, frame, command);
    media->release_frame(media->user_data, frame);
    media->close(media->user_data, resource);
    return status == FW_STATUS_OK ? FW_STATUS_OK : FW_STATUS_SINK_REJECTED;
}

static fw_status FW_CALL mr_render(fw_plugin_handle plugin,
    const fw_media_renderer_request_v1 *request,
    const fw_media_session_snapshot_v1 *snapshot, fw_rect_f32 bounds,
    const fw_media_services_v1 *services,
    fw_media_render_result_v1 *out_result) {
    fw_media_normalization_flags flags;
    fw_media_output_mode mode;
    fw_media_info_v1 info;
    fw_media_surface_command_v1 command;
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !mr_finite_nonnegative(bounds.width) ||
        !mr_finite_nonnegative(bounds.height)) return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = mr_validate_request(plugin, request, &flags, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    status = mr_validate_snapshot(request, snapshot);
    if (status != FW_STATUS_OK) return status;
    status = mr_probe(request, services, &info);
    if (status != FW_STATUS_OK) return status;
    status = mr_select_output(request, &info, &mode, &flags);
    if (status != FW_STATUS_OK) return status;
    memset(&command, 0, sizeof(command));
    command.struct_size = sizeof(command);
    command.session_id = snapshot->session_id;
    command.zone_id = request->zone_id;
    command.viewport = bounds;
    command.opacity = request->opacity;
    command.clip_to_viewport = request->placement.clip;
    command.show_poster_until_ready =
        request->poster_or_artwork_resource_id.length != 0u;
    mr_placement(info.intrinsic_visual_size, bounds, &request->placement,
        &command.source_normalized, &command.destination);
    out_result->output_mode = mode;
    out_result->destination = command.destination;
    out_result->source_normalized = command.source_normalized;
    out_result->session_revision = snapshot->revision;
    out_result->normalization_flags = flags;
    if (request->opacity > 0.0f) {
        if (mode == FW_MEDIA_OUTPUT_EXTERNAL_SURFACE &&
            request->kind == FW_MEDIA_KIND_VIDEO) {
            if (services->visual == NULL ||
                services->visual->struct_size < sizeof(*services->visual) ||
                services->visual->place_external_surface == NULL)
                return FW_STATUS_INVALID_ARGUMENT;
            status = services->visual->place_external_surface(
                services->visual->user_data, &command);
            if (status != FW_STATUS_OK) return FW_STATUS_SINK_REJECTED;
            out_result->command_count = 1u;
        } else if (mode == FW_MEDIA_OUTPUT_POSTER_ONLY) {
            if (request->poster_or_artwork_resource_id.length == 0u)
                return FW_STATUS_UNSUPPORTED;
            if (services->visual == NULL ||
                services->visual->struct_size < sizeof(*services->visual) ||
                services->visual->draw_poster == NULL)
                return FW_STATUS_INVALID_ARGUMENT;
            status = services->visual->draw_poster(services->visual->user_data,
                request->poster_or_artwork_resource_id, &command);
            if (status != FW_STATUS_OK) return FW_STATUS_SINK_REJECTED;
            out_result->command_count = 1u;
        } else if (mode == FW_MEDIA_OUTPUT_DECODED_FRAME) {
            status = mr_draw_decoded(request, snapshot, services, &command);
            if (status != FW_STATUS_OK) return status;
            out_result->command_count = 1u;
        }
    }
    mr_cache_key(request, snapshot, &info, mode, command.destination,
        command.source_normalized, &out_result->cache_key_high,
        &out_result->cache_key_low);
    return FW_STATUS_OK;
}

static fw_status FW_CALL mr_build_semantics(fw_plugin_handle plugin,
    const fw_media_renderer_request_v1 *request,
    const fw_media_session_snapshot_v1 *snapshot, fw_rect_f32 bounds,
    fw_media_semantics_v1 *out_semantics) {
    fw_media_normalization_flags flags;
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_semantics == NULL ||
        out_semantics->struct_size < sizeof(*out_semantics) ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !mr_finite_nonnegative(bounds.width) ||
        !mr_finite_nonnegative(bounds.height)) return FW_STATUS_INVALID_ARGUMENT;
    size = out_semantics->struct_size;
    memset(out_semantics, 0, sizeof(*out_semantics));
    out_semantics->struct_size = size;
    status = mr_validate_request(plugin, request, &flags, &key);
    (void)flags; (void)key;
    if (status != FW_STATUS_OK) return status;
    status = mr_validate_snapshot(request, snapshot);
    if (status != FW_STATUS_OK) return status;
    out_semantics->role = FW_SEMANTICS_ROLE_MEDIA;
    out_semantics->label = request->label;
    out_semantics->bounds = bounds;
    out_semantics->state = snapshot->state;
    out_semantics->position_ms = snapshot->position_ms;
    out_semantics->duration_ms = snapshot->duration_ms;
    out_semantics->has_duration = snapshot->duration_ms != 0u;
    out_semantics->tracks = request->tracks;
    out_semantics->track_count = request->track_count;
    out_semantics->selected_track_resource_id =
        snapshot->selected_track_resource_id;
    out_semantics->actions = snapshot->state == FW_MEDIA_STATE_PLAYING ?
        FW_MEDIA_ACTION_PAUSE : FW_MEDIA_ACTION_PLAY;
    out_semantics->actions |= FW_MEDIA_ACTION_SET_RATE |
        FW_MEDIA_ACTION_SET_MUTED | FW_MEDIA_ACTION_SET_VOLUME;
    if (out_semantics->has_duration != 0u)
        out_semantics->actions |= FW_MEDIA_ACTION_SEEK_RELATIVE |
            FW_MEDIA_ACTION_SEEK_TO;
    if (request->track_count != 0u)
        out_semantics->actions |= FW_MEDIA_ACTION_SELECT_TRACK;
    out_semantics->hidden = snapshot->hidden_from_semantics;
    return FW_STATUS_OK;
}

static fw_status FW_CALL mr_get_parameter_schema(fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!mr_context_valid(plugin) || out_schema_json == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    out_schema_json->data = mr_parameter_schema;
    out_schema_json->length = sizeof(mr_parameter_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_media_renderer_api_v1 mr_renderer_api = {
    sizeof(fw_media_renderer_api_v1), FW_MEDIA_RENDERER_INTERFACE_VERSION,
    mr_validate, mr_measure, mr_render, mr_build_semantics,
    mr_get_parameter_schema,
};

static const fw_plugin_descriptor_v1 *FW_CALL mr_get_descriptor(void) {
    return &mr_descriptor;
}

static fw_status FW_CALL mr_load(const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    mr_context *context;
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL ||
        host->struct_size < sizeof(*host) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR)
        return FW_STATUS_INVALID_ARGUMENT;
    context = (mr_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FW_STATUS_OUT_OF_MEMORY;
    context->magic = MR_MAGIC;
    context->host = *host;
    *out_handle = context;
    return FW_STATUS_OK;
}

static void FW_CALL mr_unload(fw_plugin_handle handle) {
    mr_context *context = (mr_context *)handle;
    if (context != NULL && context->magic == MR_MAGIC) {
        context->magic = 0u;
        free(context);
    }
}

static fw_status FW_CALL mr_query_interface(fw_plugin_handle handle,
    fw_string_view interface_id, uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (!mr_context_valid(handle) || !mr_string_shape(interface_id))
        return FW_STATUS_INVALID_ARGUMENT;
    if (minimum_version > FW_MEDIA_RENDERER_INTERFACE_VERSION ||
        (!mr_string_equal(interface_id, FW_VIDEO_RENDERER_INTERFACE_ID) &&
         !mr_string_equal(interface_id, FW_AUDIO_RENDERER_INTERFACE_ID)))
        return FW_STATUS_NOT_FOUND;
    *out_interface = &mr_renderer_api;
    return FW_STATUS_OK;
}

static const fw_plugin_api_v1 mr_plugin_api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    mr_get_descriptor, mr_load, mr_unload, mr_query_interface,
};

#if defined(FACETWIRE_CORE_MEDIA_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_core_media_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) return NULL;
    return &mr_plugin_api;
}

#if defined(FACETWIRE_CORE_MEDIA_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    return facetwire_core_media_plugin_query(requested_abi);
}
#endif
