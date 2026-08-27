/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/media_renderer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #value); \
    return 1; } } while (0)

typedef struct fake_state {
    fw_media_info_v1 info;
    fw_media_frame_info_v1 frame_info;
    fw_media_probe_request_v1 last_probe;
    fw_media_open_request_v1 last_open;
    fw_media_surface_command_v1 last_command;
    fw_string_view last_poster;
    uint32_t probes;
    uint32_t opens;
    uint32_t closes;
    uint32_t acquires;
    uint32_t releases;
    uint32_t external_commands;
    uint32_t frame_commands;
    uint32_t poster_commands;
    fw_status open_status;
    fw_status acquire_status;
    fw_status sink_status;
} fake_state;

static void reset_counts(fake_state *state) {
    state->probes = 0u;
    state->opens = 0u;
    state->closes = 0u;
    state->acquires = 0u;
    state->releases = 0u;
    state->external_commands = 0u;
    state->frame_commands = 0u;
    state->poster_commands = 0u;
    state->open_status = FW_STATUS_OK;
    state->acquire_status = FW_STATUS_OK;
    state->sink_status = FW_STATUS_OK;
}

static fw_status FW_CALL fake_probe(void *user_data,
    const fw_media_probe_request_v1 *request, fw_media_info_v1 *out_info) {
    fake_state *state = (fake_state *)user_data;
    ++state->probes;
    state->last_probe = *request;
    *out_info = state->info;
    return FW_STATUS_OK;
}

static fw_status FW_CALL fake_open(void *user_data,
    const fw_media_open_request_v1 *request,
    fw_media_resource_token *out_token) {
    fake_state *state = (fake_state *)user_data;
    ++state->opens;
    state->last_open = *request;
    *out_token = UINT64_C(0x101);
    return state->open_status;
}

static void FW_CALL fake_close(void *user_data,
    fw_media_resource_token token) {
    fake_state *state = (fake_state *)user_data;
    if (token != 0u) ++state->closes;
}

static fw_status FW_CALL fake_acquire(void *user_data,
    fw_media_resource_token resource, uint64_t position_ms,
    fw_media_frame_token *out_frame, fw_media_frame_info_v1 *out_info) {
    fake_state *state = (fake_state *)user_data;
    (void)position_ms;
    ++state->acquires;
    *out_frame = resource != 0u ? UINT64_C(0x202) : 0u;
    *out_info = state->frame_info;
    return state->acquire_status;
}

static void FW_CALL fake_release(void *user_data,
    fw_media_frame_token frame) {
    fake_state *state = (fake_state *)user_data;
    if (frame != 0u) ++state->releases;
}

static fw_status FW_CALL fake_external(void *user_data,
    const fw_media_surface_command_v1 *command) {
    fake_state *state = (fake_state *)user_data;
    ++state->external_commands;
    state->last_command = *command;
    return state->sink_status;
}

static fw_status FW_CALL fake_draw_frame(void *user_data,
    fw_media_frame_token frame,
    const fw_media_surface_command_v1 *command) {
    fake_state *state = (fake_state *)user_data;
    if (frame != 0u) ++state->frame_commands;
    state->last_command = *command;
    return state->sink_status;
}

static fw_status FW_CALL fake_draw_poster(void *user_data,
    fw_string_view resource_id,
    const fw_media_surface_command_v1 *command) {
    fake_state *state = (fake_state *)user_data;
    ++state->poster_commands;
    state->last_poster = resource_id;
    state->last_command = *command;
    return state->sink_status;
}

static fw_media_renderer_request_v1 make_request(fw_media_kind kind) {
    fw_media_renderer_request_v1 request;
    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.request_id = 7u;
    request.kind = kind;
    request.zone_id = (fw_string_view)FW_STRING_VIEW_LITERAL("zone:media");
    request.resource_id = kind == FW_MEDIA_KIND_VIDEO ?
        (fw_string_view)FW_STRING_VIEW_LITERAL("video.demo") :
        (fw_string_view)FW_STRING_VIEW_LITERAL("audio.demo");
    request.label = kind == FW_MEDIA_KIND_VIDEO ?
        (fw_string_view)FW_STRING_VIEW_LITERAL("Demo video") :
        (fw_string_view)FW_STRING_VIEW_LITERAL("Demo audio");
    request.title = (fw_string_view)FW_STRING_VIEW_LITERAL("FacetWire media");
    request.poster_or_artwork_resource_id = kind == FW_MEDIA_KIND_VIDEO ?
        (fw_string_view)FW_STRING_VIEW_LITERAL("poster.demo") :
        (fw_string_view){NULL, 0u};
    request.opacity = 0.65f;
    request.placement.struct_size = sizeof(request.placement);
    request.placement.fit = FW_MEDIA_FIT_CONTAIN;
    request.placement.alignment_x = 0.5f;
    request.placement.alignment_y = 0.5f;
    request.placement.clip = 1u;
    request.playback.struct_size = sizeof(request.playback);
    request.playback.volume = 1.0f;
    request.playback.playback_rate = 1.0f;
    request.playback.controls = FW_MEDIA_CONTROLS_AUTO;
    request.constraints.struct_size = sizeof(request.constraints);
    request.constraints.max_width = 640.0f;
    request.constraints.max_height = 360.0f;
    request.target.struct_size = sizeof(request.target);
    request.target.device_pixel_ratio = 1.0f;
    request.target.font_scale = 1.0f;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.target.supports_alpha = 1u;
    request.presentation_revision = 3u;
    request.flags = FW_MEDIA_REQUEST_ALLOW_POSTER_FALLBACK;
    return request;
}

static fw_media_session_snapshot_v1 make_snapshot(void) {
    fw_media_session_snapshot_v1 snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.struct_size = sizeof(snapshot);
    snapshot.session_id = 42u;
    snapshot.revision = 3u;
    snapshot.state = FW_MEDIA_STATE_PLAYING;
    snapshot.position_ms = 750u;
    snapshot.duration_ms = 9000u;
    snapshot.buffered_until_ms = 4000u;
    snapshot.playback_rate = 1.0f;
    snapshot.effective_volume = 0.8f;
    return snapshot;
}

static void init_fake(fake_state *state) {
    memset(state, 0, sizeof(*state));
    state->info.struct_size = sizeof(state->info);
    state->info.media_type =
        (fw_string_view)FW_STRING_VIEW_LITERAL("video/mp4");
    state->info.intrinsic_visual_size =
        (fw_size_f32){1920.0f, 1080.0f};
    state->info.duration_ms = 9000u;
    state->info.has_duration = 1u;
    state->info.available_output_modes = FW_MEDIA_OUTPUT_ALL;
    state->info.has_audio = 1u;
    state->info.has_video = 1u;
    state->info.fingerprint_high = 11u;
    state->info.fingerprint_low = 22u;
    state->frame_info.struct_size = sizeof(state->frame_info);
    state->frame_info.visual_size = (fw_size_f32){1920.0f, 1080.0f};
    state->frame_info.timestamp_ms = 750u;
    state->frame_info.duration_ms = 33u;
    reset_counts(state);
}

int main(void) {
    const fw_plugin_api_v1 *api =
        facetwire_core_media_plugin_query(FW_ABI_VERSION_CURRENT);
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    fw_plugin_handle plugin = NULL;
    fw_plugin_handle failed_plugin = (fw_plugin_handle)(uintptr_t)1u;
    const void *iface = NULL;
    const fw_media_renderer_api_v1 *renderer;
    fw_media_renderer_request_v1 request = make_request(FW_MEDIA_KIND_VIDEO);
    fw_media_session_snapshot_v1 snapshot = make_snapshot();
    fw_media_validation_result_v1 validation = {0};
    fw_media_measure_result_v1 measure = {0};
    fw_media_render_result_v1 render = {0};
    fw_media_render_result_v1 render_copy = {0};
    fw_media_semantics_v1 semantics = {0};
    fw_media_track_v1 tracks[2];
    fw_string_view schema = {0};
    fake_state state;
    fw_media_service_v1 media;
    fw_media_visual_sink_v1 visual;
    fw_media_services_v1 services;
    char resource_copy[] = "video.demo";

    init_fake(&state);
    media = (fw_media_service_v1){sizeof(media), &state, fake_probe,
        fake_open, fake_close, fake_acquire, fake_release};
    visual = (fw_media_visual_sink_v1){sizeof(visual), &state,
        fake_external, fake_draw_frame, fake_draw_poster};
    services = (fw_media_services_v1){sizeof(services), &media, &visual, 0u};

    CHECK(api != NULL);
    CHECK(api->load(NULL, &failed_plugin) == FW_STATUS_INVALID_ARGUMENT);
    CHECK(failed_plugin == NULL);
    CHECK(api != NULL && api->load(&host, &plugin) == FW_STATUS_OK);
    CHECK(api->get_descriptor()->capability_count == 2u);
    CHECK(api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_VIDEO_RENDERER_INTERFACE_ID),
        1u, &iface) == FW_STATUS_OK);
    renderer = (const fw_media_renderer_api_v1 *)iface;
    CHECK(renderer->interface_version == FW_MEDIA_RENDERER_INTERFACE_VERSION);
    iface = NULL;
    CHECK(api->query_interface(plugin,
        (fw_string_view)FW_STRING_VIEW_LITERAL(FW_AUDIO_RENDERER_INTERFACE_ID),
        1u, &iface) == FW_STATUS_OK && iface == renderer);

    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) == FW_STATUS_OK);
    CHECK(state.probes == 0u);

    measure.struct_size = sizeof(measure);
    CHECK(renderer->measure(plugin, &request, &services, &measure) ==
        FW_STATUS_OK);
    CHECK(state.probes == 1u);
    CHECK(measure.selected_output_mode == FW_MEDIA_OUTPUT_EXTERNAL_SURFACE);
    CHECK(fabsf(measure.size.width - 640.0f) < 0.001f);
    CHECK(fabsf(measure.size.height - 360.0f) < 0.001f);

    reset_counts(&state);
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){10.0f, 20.0f, 400.0f, 300.0f},
        &services, &render) == FW_STATUS_OK);
    CHECK(render.output_mode == FW_MEDIA_OUTPUT_EXTERNAL_SURFACE);
    CHECK(state.probes == 1u && state.external_commands == 1u);
    CHECK(state.opens == 0u && state.acquires == 0u);
    CHECK(fabsf(state.last_command.opacity - 0.65f) < 0.001f);
    CHECK(render.command_count == 1u);

    request.resource_id.data = resource_copy;
    render_copy.struct_size = sizeof(render_copy);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){10.0f, 20.0f, 400.0f, 300.0f},
        &services, &render_copy) == FW_STATUS_OK);
    CHECK(render.cache_key_high == render_copy.cache_key_high &&
        render.cache_key_low == render_copy.cache_key_low);
    request = make_request(FW_MEDIA_KIND_VIDEO);

    reset_counts(&state);
    state.info.available_output_modes = FW_MEDIA_OUTPUT_DECODED_FRAME;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &services, &render) == FW_STATUS_OK);
    CHECK(render.output_mode == FW_MEDIA_OUTPUT_DECODED_FRAME);
    CHECK(state.opens == 1u && state.closes == 1u);
    CHECK(state.acquires == 1u && state.releases == 1u);
    CHECK(state.frame_commands == 1u);

    reset_counts(&state);
    state.open_status = FW_STATUS_PLUGIN_ERROR;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &services, &render) == FW_STATUS_PLUGIN_ERROR);
    CHECK(state.opens == 1u && state.closes == 1u);
    CHECK(state.acquires == 0u && state.releases == 0u);

    reset_counts(&state);
    state.acquire_status = FW_STATUS_PLUGIN_ERROR;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &services, &render) == FW_STATUS_PLUGIN_ERROR);
    CHECK(state.opens == state.closes && state.acquires == state.releases);

    reset_counts(&state);
    state.sink_status = FW_STATUS_PLUGIN_ERROR;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &services, &render) == FW_STATUS_SINK_REJECTED);
    CHECK(state.opens == state.closes && state.acquires == state.releases);

    reset_counts(&state);
    state.info.available_output_modes =
        FW_MEDIA_OUTPUT_POSTER_ONLY | FW_MEDIA_OUTPUT_DECODED_FRAME;
    request.target.medium = FW_RENDER_MEDIUM_EXPORT;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &services, &render) == FW_STATUS_OK);
    CHECK(render.output_mode == FW_MEDIA_OUTPUT_POSTER_ONLY);
    CHECK(state.poster_commands == 1u && state.opens == 0u);
    CHECK(state.last_poster.length == request.poster_or_artwork_resource_id.length);

    reset_counts(&state);
    state.info.available_output_modes = FW_MEDIA_OUTPUT_EXTERNAL_SURFACE;
    request.target.medium = FW_RENDER_MEDIUM_SCREEN;
    request.opacity = 0.0f;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &services, &render) == FW_STATUS_OK);
    CHECK(render.command_count == 0u && state.external_commands == 0u);
    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 640.0f, 360.0f},
        &semantics) == FW_STATUS_OK);
    CHECK(semantics.role == FW_SEMANTICS_ROLE_MEDIA);
    CHECK((semantics.actions & FW_MEDIA_ACTION_PAUSE) != 0u);
    CHECK((semantics.actions & FW_MEDIA_ACTION_SEEK_TO) != 0u);

    reset_counts(&state);
    request = make_request(FW_MEDIA_KIND_AUDIO);
    state.info.media_type =
        (fw_string_view)FW_STRING_VIEW_LITERAL("audio/ogg");
    state.info.intrinsic_visual_size = (fw_size_f32){0.0f, 0.0f};
    state.info.available_output_modes = FW_MEDIA_OUTPUT_EXTERNAL_SURFACE;
    state.info.has_audio = 1u;
    state.info.has_video = 0u;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 300.0f, 80.0f},
        &services, &render) == FW_STATUS_OK);
    CHECK(render.output_mode == FW_MEDIA_OUTPUT_EXTERNAL_SURFACE);
    CHECK(render.command_count == 0u && state.external_commands == 0u);

    reset_counts(&state);
    state.info.available_output_modes = FW_MEDIA_OUTPUT_DECODED_FRAME;
    render.struct_size = sizeof(render);
    CHECK(renderer->render(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 300.0f, 80.0f},
        &services, &render) == FW_STATUS_UNSUPPORTED);
    CHECK(state.opens == 0u && state.acquires == 0u);

    memset(tracks, 0, sizeof(tracks));
    tracks[0].struct_size = sizeof(tracks[0]);
    tracks[0].resource_id =
        (fw_string_view)FW_STRING_VIEW_LITERAL("track.zh");
    tracks[0].kind = (fw_string_view)FW_STRING_VIEW_LITERAL("subtitles");
    tracks[0].language = (fw_string_view)FW_STRING_VIEW_LITERAL("zh-CN");
    tracks[0].is_default = 1u;
    tracks[1] = tracks[0];
    tracks[1].resource_id =
        (fw_string_view)FW_STRING_VIEW_LITERAL("track.en");
    request.tracks = tracks;
    request.track_count = 2u;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);

    request = make_request(FW_MEDIA_KIND_VIDEO);
    request.opacity = -0.01f;
    validation.struct_size = sizeof(validation);
    CHECK(renderer->validate(plugin, &request, &validation) ==
        FW_STATUS_INVALID_ARGUMENT);
    request = make_request(FW_MEDIA_KIND_VIDEO);
    snapshot.revision = 2u;
    semantics.struct_size = sizeof(semantics);
    CHECK(renderer->build_semantics(plugin, &request, &snapshot,
        (fw_rect_f32){0.0f, 0.0f, 1.0f, 1.0f},
        &semantics) == FW_STATUS_INVALID_STATE);

    CHECK(renderer->get_parameter_schema(plugin, &schema) == FW_STATUS_OK);
    CHECK(schema.data != NULL && schema.length > 20u);
    api->unload(plugin);
    puts("core media renderer contract passed");
    return 0;
}
