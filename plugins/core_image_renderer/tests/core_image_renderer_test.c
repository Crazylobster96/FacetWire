/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/image_renderer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #value); \
    return 1; } } while (0)

typedef struct fake_image {
    uint32_t marker;
} fake_image;

typedef struct fake_state {
    uint32_t acquires;
    uint32_t releases;
    uint32_t saves;
    uint32_t restores;
    uint32_t clips;
    uint32_t draws;
    float opacity;
    fw_rect_f32 destination;
    fw_image_acquire_request_v1 acquire_request;
} fake_state;

static fw_status FW_CALL fake_acquire(void *user_data,
    const fw_image_acquire_request_v1 *request,
    fw_image_handle *out_handle, fw_image_info_v1 *out_info) {
    fake_state *state = (fake_state *)user_data;
    fake_image *image = (fake_image *)malloc(sizeof(*image));
    if (image == NULL) return FW_STATUS_OUT_OF_MEMORY;
    image->marker = 42u;
    ++state->acquires;
    state->acquire_request = *request;
    *out_handle = image;
    out_info->intrinsic_size = (fw_size_f32){640.0f, 360.0f};
    out_info->frame_count = request->kind == FW_IMAGE_CONTENT_ANIMATED ? 12u : 1u;
    out_info->frame_index = request->kind == FW_IMAGE_CONTENT_ANIMATED ? 4u : 0u;
    out_info->duration_ms = request->kind == FW_IMAGE_CONTENT_ANIMATED ? 1200u : 0u;
    out_info->has_alpha = 1u;
    out_info->media_type = request->kind == FW_IMAGE_CONTENT_ANIMATED ?
        (fw_string_view)FW_STRING_VIEW_LITERAL("image/gif") :
        (fw_string_view)FW_STRING_VIEW_LITERAL("image/png");
    out_info->fingerprint_high = 11u;
    out_info->fingerprint_low = 22u;
    return FW_STATUS_OK;
}
static void FW_CALL fake_release(void *user_data, fw_image_handle handle) {
    ++((fake_state *)user_data)->releases; free(handle);
}
static fw_status FW_CALL fake_save(void *u) {
    ++((fake_state *)u)->saves; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_restore(void *u) {
    ++((fake_state *)u)->restores; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_clip(void *u, fw_rect_f32 rect) {
    (void)rect; ++((fake_state *)u)->clips; return FW_STATUS_OK;
}
static fw_status FW_CALL fake_draw(void *u, fw_image_handle handle,
    fw_rect_f32 source, fw_rect_f32 destination, float opacity,
    fw_image_sampling sampling) {
    fake_state *state = (fake_state *)u;
    CHECK(handle != NULL);
    (void)source; (void)sampling;
    ++state->draws; state->opacity = opacity; state->destination = destination;
    return FW_STATUS_OK;
}

static fw_image_renderer_request_v1 make_request(fw_image_content_kind kind) {
    fw_image_renderer_request_v1 request;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:image");
    request.resource_id = (fw_string_view)FW_STRING_VIEW_LITERAL("image.demo");
    request.alt = (fw_string_view)FW_STRING_VIEW_LITERAL("Opacity demo image");
    request.kind = kind;
    request.opacity = 0.35f;
    request.placement.struct_size = sizeof(request.placement);
    request.placement.fit = FW_IMAGE_FIT_COVER;
    request.placement.alignment_x = 0.5f;
    request.placement.alignment_y = 0.5f;
    request.placement.clip = 1u;
    request.placement.sampling = FW_IMAGE_SAMPLING_SMOOTH;
    request.playback.struct_size = sizeof(request.playback);
    request.playback.autoplay = 1u;
    request.playback.loop = 1u;
    request.playback.playback_rate = 1.0f;
    request.playback.position_ms = 450u;
    request.playback.playing = 1u;
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.max_width = 320.0f;
    request.constraints.max_height = 180.0f;
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    return request;
}

int main(void) {
    const fw_plugin_api_v1 *api =
        facetwire_core_image_plugin_query(FW_ABI_VERSION_CURRENT);
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle plugin = NULL;
    const void *iface = NULL;
    const fw_image_renderer_api_v1 *renderer;
    fw_image_renderer_request_v1 request = make_request(FW_IMAGE_CONTENT_STATIC);
    fw_image_validation_result_v1 validation = {0};
    fw_image_measure_result_v1 measure = {0};
    fw_image_render_result_v1 render = {0};
    fw_image_semantics_v1 semantics = {0};
    fake_state state = {0};
    fw_image_service_v1 images = {sizeof(images), &state, fake_acquire, fake_release};
    fw_image_draw_sink_v1 draw = {
        sizeof(draw), &state, fake_save, fake_restore, fake_clip, fake_draw};
    fw_image_services_v1 services = {sizeof(services), &images, &draw, 0u};
    CHECK(api != NULL && api->load(&host, &plugin) == FW_STATUS_OK);
    CHECK(api->get_descriptor()->capability_count == 2u);
    CHECK(api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_IMAGE_RENDERER_INTERFACE_ID),
        1u, &iface) == FW_STATUS_OK);
    renderer = (const fw_image_renderer_api_v1 *)iface;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) == FW_STATUS_OK);
    measure.struct_size = sizeof(measure);
    CHECK(renderer->measure(plugin, &request, &services, &measure) == FW_STATUS_OK);
    CHECK(fabsf(measure.size.width - 320.0f) < 0.001f);
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request,
        (fw_rect_f32){0, 0, 300, 300}, &services, &render) == FW_STATUS_OK);
    CHECK(state.saves == 1u && state.clips == 1u && state.draws == 1u &&
        state.restores == 1u);
    CHECK(fabsf(state.opacity - 0.35f) < 0.001f);
    CHECK(state.destination.width > 300.0f || state.destination.height > 300.0f);
    CHECK(state.acquires == state.releases);
    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(plugin, &request,
        (fw_rect_f32){0, 0, 300, 300}, &semantics) == FW_STATUS_OK);
    CHECK(semantics.decorative == 0u && semantics.animated == 0u);
    request = make_request(FW_IMAGE_CONTENT_ANIMATED);
    request.target.reduce_motion = 1u;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request,
        (fw_rect_f32){0, 0, 320, 180}, &services, &render) == FW_STATUS_OK);
    CHECK(state.acquire_request.position_ms == 0u);
    CHECK(render.frame_count == 12u && render.frame_index == 4u);
    request.opacity = -0.1f;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);
    api->unload(plugin);
    puts("core image renderer contract passed");
    return 0;
}
