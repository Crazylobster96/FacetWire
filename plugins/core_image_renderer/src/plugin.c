/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/image_renderer.h>

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define IM_MAGIC 0x494d4731u
#define IM_MAX_STRING_BYTES 8192u
#define IM_FIELD_END(type, field) \
    (offsetof(type, field) + sizeof(((type *)0)->field))
#define IM_REQUEST_V1_BASE_SIZE \
    IM_FIELD_END(fw_image_renderer_request_v1, flags)
#define IM_DRAW_SINK_V1_BASE_SIZE \
    IM_FIELD_END(fw_image_draw_sink_v1, draw_image)
#define IM_RENDER_RESULT_V1_BASE_SIZE \
    IM_FIELD_END(fw_image_render_result_v1, flags)
#define IM_SEMANTICS_V1_BASE_SIZE \
    IM_FIELD_END(fw_image_semantics_v1, flags)

typedef struct im_context {
    uint32_t magic;
    fw_host_api_v1 host;
} im_context;

static const fw_capability_descriptor_v1 im_capabilities[] = {
    {sizeof(fw_capability_descriptor_v1),
     FW_STRING_VIEW_LITERAL(FW_IMAGE_RENDERER_CAPABILITY_ID),
     FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
     FW_RENDERER_FLAG_DETERMINISTIC | FW_RENDERER_FLAG_HEADLESS |
         FW_RENDERER_FLAG_SEMANTICS},
    {sizeof(fw_capability_descriptor_v1),
     FW_STRING_VIEW_LITERAL(FW_ANIMATED_IMAGE_RENDERER_CAPABILITY_ID),
     FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
     FW_RENDERER_FLAG_HEADLESS | FW_RENDERER_FLAG_SEMANTICS},
};

static const fw_plugin_descriptor_v1 im_descriptor = {
    sizeof(fw_plugin_descriptor_v1), FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.core-image-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Core Image Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    im_capabilities, sizeof(im_capabilities) / sizeof(im_capabilities[0]),
};

static const char im_parameter_schema[] =
    "{\"schemaVersion\":1,\"parameters\":["
    "{\"id\":\"opacity\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0,\"maximum\":1,\"step\":0.01},"
    "{\"id\":\"fit\",\"type\":\"enum\",\"default\":\"contain\","
    "\"values\":[\"none\",\"contain\",\"cover\",\"fill\"]},"
    "{\"id\":\"playbackRate\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0.05,\"maximum\":16},"
    "{\"id\":\"contentRotationQuarterTurns\",\"type\":\"integer\","
    "\"default\":0,\"minimum\":0,\"maximum\":3}]}";

static int im_context_valid(fw_plugin_handle plugin) {
    const im_context *context = (const im_context *)plugin;
    return context != NULL && context->magic == IM_MAGIC;
}

static fw_string_view im_view(const char *value) {
    fw_string_view result = {value, strlen(value)};
    return result;
}

static int im_string_shape(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int im_string_equal(fw_string_view value, const char *literal) {
    const size_t length = strlen(literal);
    return value.length == length &&
        (length == 0u || memcmp(value.data, literal, length) == 0);
}

static int im_utf8_valid(fw_string_view value) {
    size_t i = 0u;
    while (i < value.length) {
        const unsigned char first = (unsigned char)value.data[i++];
        size_t count;
        uint32_t cp;
        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) { count = 1u; cp = first & 0x1fu; }
        else if (first >= 0xe0u && first <= 0xefu) { count = 2u; cp = first & 0x0fu; }
        else if (first >= 0xf0u && first <= 0xf4u) { count = 3u; cp = first & 0x07u; }
        else return 0;
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

static int im_finite_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
}

static fw_visual_rotation_quarter_turns im_request_rotation(
    const fw_image_renderer_request_v1 *request) {
    return request->struct_size >= IM_FIELD_END(fw_image_renderer_request_v1,
        content_rotation_quarter_turns) ?
        request->content_rotation_quarter_turns : FW_VISUAL_ROTATION_0;
}

static fw_visual_transform_v1 im_transform(
    const fw_image_renderer_request_v1 *request) {
    fw_visual_transform_v1 transform;
    memset(&transform, 0, sizeof(transform));
    transform.struct_size = sizeof(transform);
    transform.fit = request->placement.fit;
    transform.alignment_x = request->placement.alignment_x;
    transform.alignment_y = request->placement.alignment_y;
    transform.clip = request->placement.clip;
    transform.content_rotation_quarter_turns =
        im_request_rotation(request);
    return transform;
}

static fw_status im_validate_request(
    fw_plugin_handle plugin,
    const fw_image_renderer_request_v1 *request,
    const char **out_key) {
    *out_key = "image.invalid_argument";
    if (!im_context_valid(plugin) || request == NULL ||
        request->struct_size < IM_REQUEST_V1_BASE_SIZE ||
        request->placement.struct_size < sizeof(request->placement) ||
        request->playback.struct_size < sizeof(request->playback) ||
        request->constraints.struct_size < sizeof(request->constraints) ||
        request->target.struct_size < sizeof(request->target)) {
        *out_key = "image.invalid_struct_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!im_string_shape(request->zone_id) ||
        !im_string_shape(request->resource_id) ||
        !im_string_shape(request->alt) || request->resource_id.length == 0u ||
        request->zone_id.length > IM_MAX_STRING_BYTES ||
        request->resource_id.length > IM_MAX_STRING_BYTES ||
        request->alt.length > IM_MAX_STRING_BYTES ||
        !im_utf8_valid(request->zone_id) ||
        !im_utf8_valid(request->resource_id) || !im_utf8_valid(request->alt)) {
        *out_key = "image.invalid_string";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->kind < FW_IMAGE_CONTENT_STATIC ||
        request->kind > FW_IMAGE_CONTENT_ANIMATED ||
        request->placement.fit > FW_IMAGE_FIT_FILL ||
        request->placement.sampling > FW_IMAGE_SAMPLING_PIXELATED ||
        request->placement.clip > 1u || request->playback.autoplay > 1u ||
        request->playback.loop > 1u || request->playback.playing > 1u ||
        im_request_rotation(request) > FW_VISUAL_ROTATION_270) {
        *out_key = "image.invalid_enum";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->opacity) || request->opacity < 0.0f ||
        request->opacity > 1.0f ||
        !isfinite(request->placement.alignment_x) ||
        !isfinite(request->placement.alignment_y) ||
        request->placement.alignment_x < 0.0f ||
        request->placement.alignment_x > 1.0f ||
        request->placement.alignment_y < 0.0f ||
        request->placement.alignment_y > 1.0f ||
        !isfinite(request->playback.playback_rate) ||
        request->playback.playback_rate <= 0.0f ||
        !im_finite_nonnegative(request->constraints.min_width) ||
        !im_finite_nonnegative(request->constraints.max_width) ||
        !im_finite_nonnegative(request->constraints.min_height) ||
        !im_finite_nonnegative(request->constraints.max_height) ||
        request->constraints.max_width < request->constraints.min_width ||
        request->constraints.max_height < request->constraints.min_height) {
        *out_key = "image.invalid_geometry";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_key = "image.valid";
    return FW_STATUS_OK;
}

static fw_status im_acquire(
    const fw_image_renderer_request_v1 *request,
    const fw_image_service_v1 *service,
    fw_image_handle *out_handle,
    fw_image_info_v1 *out_info) {
    fw_image_acquire_request_v1 acquire;
    fw_status status;
    if (service == NULL || service->struct_size < sizeof(*service) ||
        service->acquire == NULL || service->release == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    memset(&acquire, 0, sizeof(acquire));
    acquire.struct_size = sizeof(acquire);
    acquire.resource_id = request->resource_id;
    acquire.kind = request->kind;
    acquire.position_ms = request->target.reduce_motion != 0u ? 0u :
        request->playback.position_ms;
    acquire.reduce_motion = request->target.reduce_motion;
    *out_handle = NULL;
    memset(out_info, 0, sizeof(*out_info));
    out_info->struct_size = sizeof(*out_info);
    status = service->acquire(service->user_data, &acquire,
        out_handle, out_info);
    if (status != FW_STATUS_OK || *out_handle == NULL ||
        !isfinite(out_info->intrinsic_size.width) ||
        !isfinite(out_info->intrinsic_size.height) ||
        out_info->intrinsic_size.width <= 0.0f ||
        out_info->intrinsic_size.height <= 0.0f ||
        out_info->frame_count == 0u ||
        out_info->frame_index >= out_info->frame_count) {
        if (*out_handle != NULL) service->release(service->user_data, *out_handle);
        *out_handle = NULL;
        return status == FW_STATUS_OK ? FW_STATUS_PLUGIN_ERROR : status;
    }
    return FW_STATUS_OK;
}

static float im_clamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static fw_size_f32 im_measure_size(
    fw_size_f32 intrinsic,
    const fw_layout_constraints_v1 *constraints) {
    fw_size_f32 size;
    const float max_width = constraints->max_width > 0.0f ?
        constraints->max_width : intrinsic.width;
    const float max_height = constraints->max_height > 0.0f ?
        constraints->max_height : intrinsic.height;
    float scale = 1.0f;
    if (intrinsic.width > max_width) scale = max_width / intrinsic.width;
    if (intrinsic.height * scale > max_height)
        scale = max_height / intrinsic.height;
    size.width = im_clamp(intrinsic.width * scale,
        constraints->min_width, max_width);
    size.height = im_clamp(intrinsic.height * scale,
        constraints->min_height, max_height);
    return size;
}

static void im_hash(uint64_t *high, uint64_t *low,
    const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0u; i < length; ++i) {
        *low ^= bytes[i]; *low *= UINT64_C(1099511628211);
        *high ^= (*low >> 29u) ^ bytes[i];
        *high *= UINT64_C(14029467366897019727);
    }
}

static fw_status FW_CALL im_validate(fw_plugin_handle plugin,
    const fw_image_renderer_request_v1 *request,
    fw_image_validation_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = im_validate_request(plugin, request, &key);
    out_result->status = status;
    out_result->diagnostic_key = im_view(key);
    return status;
}

static fw_status FW_CALL im_measure(fw_plugin_handle plugin,
    const fw_image_renderer_request_v1 *request,
    const fw_image_services_v1 *services,
    fw_image_measure_result_v1 *out_result) {
    const char *key;
    fw_image_handle handle;
    fw_image_info_v1 info;
    fw_visual_transform_v1 transform;
    fw_visual_transform_result_v1 resolved;
    fw_status status;
    uint32_t size;
    (void)key;
    if (services == NULL || services->struct_size < sizeof(*services) ||
        out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = im_validate_request(plugin, request, &key);
    if (status != FW_STATUS_OK) return status;
    status = im_acquire(request, services->images, &handle, &info);
    if (status != FW_STATUS_OK) return status;
    out_result->intrinsic_size = info.intrinsic_size;
    transform = im_transform(request);
    transform.fit = FW_VISUAL_FIT_NONE;
    memset(&resolved, 0, sizeof(resolved));
    resolved.struct_size = sizeof(resolved);
    status = fw_visual_transform_resolve(info.intrinsic_size,
        (fw_rect_f32){0.0f, 0.0f, info.intrinsic_size.width,
            info.intrinsic_size.height}, &transform, &resolved);
    if (status != FW_STATUS_OK) {
        services->images->release(services->images->user_data, handle);
        return status;
    }
    out_result->size = im_measure_size(resolved.effective_intrinsic_size,
        &request->constraints);
    services->images->release(services->images->user_data, handle);
    return FW_STATUS_OK;
}

static fw_status FW_CALL im_render(fw_plugin_handle plugin,
    const fw_image_renderer_request_v1 *request,
    fw_rect_f32 bounds,
    const fw_image_services_v1 *services,
    fw_image_render_result_v1 *out_result) {
    const char *key;
    fw_image_handle handle;
    fw_image_info_v1 info;
    const fw_image_draw_sink_v1 *draw;
    fw_visual_transform_v1 transform;
    fw_visual_transform_result_v1 resolved;
    fw_visual_rotation_quarter_turns rotation;
    int has_transformed_draw;
    fw_status status;
    fw_status first = FW_STATUS_OK;
    uint32_t saved = 0u;
    uint32_t size;
    uint64_t high = UINT64_C(7809847782465536322);
    uint64_t low = UINT64_C(1469598103934665603);
    (void)key;
    if (services == NULL || services->struct_size < sizeof(*services) ||
        out_result == NULL ||
        out_result->struct_size < IM_RENDER_RESULT_V1_BASE_SIZE ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !im_finite_nonnegative(bounds.width) ||
        !im_finite_nonnegative(bounds.height)) return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, size < sizeof(*out_result) ?
        size : sizeof(*out_result));
    out_result->struct_size = size;
    status = im_validate_request(plugin, request, &key);
    if (status != FW_STATUS_OK) return status;
    rotation = im_request_rotation(request);
    draw = services->draw;
    if (draw == NULL || draw->struct_size < IM_DRAW_SINK_V1_BASE_SIZE ||
        draw->save == NULL || draw->restore == NULL ||
        draw->clip_rect == NULL || draw->draw_image == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    has_transformed_draw = draw->struct_size >=
        IM_FIELD_END(fw_image_draw_sink_v1, draw_image_transformed) &&
        draw->draw_image_transformed != NULL;
    if (rotation != FW_VISUAL_ROTATION_0 && !has_transformed_draw)
        return FW_STATUS_UNSUPPORTED;
    status = im_acquire(request, services->images, &handle, &info);
    if (status != FW_STATUS_OK) return status;
    transform = im_transform(request);
    memset(&resolved, 0, sizeof(resolved));
    resolved.struct_size = sizeof(resolved);
    status = fw_visual_transform_resolve(info.intrinsic_size, bounds,
        &transform, &resolved);
    if (status != FW_STATUS_OK) {
        services->images->release(services->images->user_data, handle);
        return status;
    }
    out_result->source_rect = (fw_rect_f32){
        resolved.source_normalized.x * info.intrinsic_size.width,
        resolved.source_normalized.y * info.intrinsic_size.height,
        resolved.source_normalized.width * info.intrinsic_size.width,
        resolved.source_normalized.height * info.intrinsic_size.height};
    out_result->destination_rect = resolved.destination;
    if (size >= IM_FIELD_END(fw_image_render_result_v1,
        content_rotation_quarter_turns))
        out_result->content_rotation_quarter_turns = rotation;
    if (size >= IM_FIELD_END(fw_image_render_result_v1,
        uncovered_is_transparent))
        out_result->uncovered_is_transparent =
            resolved.uncovered_is_transparent;
    if (request->opacity > 0.0f) {
        first = draw->save(draw->user_data);
        if (first == FW_STATUS_OK) { saved = 1u; ++out_result->emitted_command_count; }
        if (first == FW_STATUS_OK && request->placement.clip != 0u) {
            first = draw->clip_rect(draw->user_data, bounds);
            if (first == FW_STATUS_OK) ++out_result->emitted_command_count;
        }
        if (first == FW_STATUS_OK) {
            if (has_transformed_draw) {
                first = draw->draw_image_transformed(draw->user_data, handle,
                    &resolved, request->opacity,
                    request->placement.sampling);
            } else {
                first = draw->draw_image(draw->user_data, handle,
                    out_result->source_rect, out_result->destination_rect,
                    request->opacity, request->placement.sampling);
            }
            if (first == FW_STATUS_OK) ++out_result->emitted_command_count;
        }
        if (saved != 0u) {
            const fw_status restored = draw->restore(draw->user_data);
            if (restored == FW_STATUS_OK) ++out_result->emitted_command_count;
            else if (first == FW_STATUS_OK) first = restored;
        }
    }
    out_result->frame_index = info.frame_index;
    out_result->frame_count = info.frame_count;
    im_hash(&high, &low, request->resource_id.data, request->resource_id.length);
    im_hash(&high, &low, &request->opacity, sizeof(request->opacity));
    im_hash(&high, &low, &request->placement, sizeof(request->placement));
    im_hash(&high, &low, &rotation, sizeof(rotation));
    im_hash(&high, &low, &bounds, sizeof(bounds));
    im_hash(&high, &low, &info.fingerprint_high, sizeof(info.fingerprint_high));
    im_hash(&high, &low, &info.frame_index, sizeof(info.frame_index));
    out_result->cache_key_high = high;
    out_result->cache_key_low = low;
    services->images->release(services->images->user_data, handle);
    return first == FW_STATUS_OK ? FW_STATUS_OK : FW_STATUS_SINK_REJECTED;
}

static fw_status FW_CALL im_build_semantics(fw_plugin_handle plugin,
    const fw_image_renderer_request_v1 *request, fw_rect_f32 bounds,
    fw_image_semantics_v1 *out_semantics) {
    const char *key;
    fw_status status;
    uint32_t size;
    (void)key;
    if (out_semantics == NULL ||
        out_semantics->struct_size < IM_SEMANTICS_V1_BASE_SIZE ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !im_finite_nonnegative(bounds.width) ||
        !im_finite_nonnegative(bounds.height)) return FW_STATUS_INVALID_ARGUMENT;
    size = out_semantics->struct_size;
    memset(out_semantics, 0, size < sizeof(*out_semantics) ?
        size : sizeof(*out_semantics));
    out_semantics->struct_size = size;
    status = im_validate_request(plugin, request, &key);
    if (status != FW_STATUS_OK) return status;
    out_semantics->role = FW_SEMANTICS_ROLE_IMAGE;
    out_semantics->label = request->alt;
    out_semantics->bounds = bounds;
    out_semantics->decorative = request->alt.length == 0u;
    out_semantics->animated = request->kind == FW_IMAGE_CONTENT_ANIMATED;
    if (size >= IM_FIELD_END(fw_image_semantics_v1,
        content_rotation_quarter_turns))
        out_semantics->content_rotation_quarter_turns =
            im_request_rotation(request);
    return FW_STATUS_OK;
}

static fw_status FW_CALL im_get_parameter_schema(fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!im_context_valid(plugin) || out_schema_json == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    out_schema_json->data = im_parameter_schema;
    out_schema_json->length = sizeof(im_parameter_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_image_renderer_api_v1 im_renderer_api = {
    sizeof(fw_image_renderer_api_v1), FW_IMAGE_RENDERER_INTERFACE_VERSION,
    im_validate, im_measure, im_render, im_build_semantics,
    im_get_parameter_schema,
};

static const fw_plugin_descriptor_v1 *FW_CALL im_get_descriptor(void) {
    return &im_descriptor;
}
static fw_status FW_CALL im_load(const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    im_context *context;
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL || host->struct_size < sizeof(*host) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR)
        return FW_STATUS_INVALID_ARGUMENT;
    context = (im_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FW_STATUS_OUT_OF_MEMORY;
    context->magic = IM_MAGIC; context->host = *host; *out_handle = context;
    return FW_STATUS_OK;
}
static void FW_CALL im_unload(fw_plugin_handle handle) {
    im_context *context = (im_context *)handle;
    if (context != NULL && context->magic == IM_MAGIC) {
        context->magic = 0u; free(context);
    }
}
static fw_status FW_CALL im_query_interface(fw_plugin_handle handle,
    fw_string_view id, uint32_t minimum_version, const void **out_interface) {
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (!im_context_valid(handle) || !im_string_shape(id))
        return FW_STATUS_INVALID_ARGUMENT;
    if (minimum_version > FW_IMAGE_RENDERER_INTERFACE_VERSION ||
        (!im_string_equal(id, FW_IMAGE_RENDERER_INTERFACE_ID) &&
         !im_string_equal(id, FW_ANIMATED_IMAGE_RENDERER_INTERFACE_ID)))
        return FW_STATUS_NOT_FOUND;
    *out_interface = &im_renderer_api;
    return FW_STATUS_OK;
}
static const fw_plugin_api_v1 im_plugin_api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    im_get_descriptor, im_load, im_unload, im_query_interface,
};

#if defined(FACETWIRE_CORE_IMAGE_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_core_image_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) return NULL;
    return &im_plugin_api;
}

#if defined(FACETWIRE_CORE_IMAGE_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    return facetwire_core_image_plugin_query(requested_abi);
}
#endif
