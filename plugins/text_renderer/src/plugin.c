/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/text_renderer.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TX_MAGIC 0x54585231u
#define TX_MAX_TEXT_BYTES (16u * 1024u * 1024u)

typedef struct tx_context {
    uint32_t magic;
    fw_host_api_v1 host;
} tx_context;

static const fw_capability_descriptor_v1 tx_capabilities[] = {{
    sizeof(fw_capability_descriptor_v1),
    FW_STRING_VIEW_LITERAL(FW_TEXT_RENDERER_CAPABILITY_ID),
    FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
    FW_RENDERER_FLAG_DETERMINISTIC | FW_RENDERER_FLAG_HEADLESS |
        FW_RENDERER_FLAG_SEMANTICS,
}};

static const fw_plugin_descriptor_v1 tx_descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.text-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Text Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    tx_capabilities,
    sizeof(tx_capabilities) / sizeof(tx_capabilities[0]),
};

static const char tx_parameter_schema[] =
    "{\"schemaVersion\":1,\"parameters\":["
    "{\"id\":\"opacity\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0,\"maximum\":1,\"step\":0.01,\"scope\":\"document\"},"
    "{\"id\":\"fontSize\",\"type\":\"number\",\"default\":16,"
    "\"exclusiveMinimum\":0,\"scope\":\"document\"},"
    "{\"id\":\"scrollOffsetY\",\"type\":\"number\",\"default\":0,"
    "\"minimum\":0,\"scope\":\"session\"}]}";

static int tx_context_valid(fw_plugin_handle plugin) {
    const tx_context *context = (const tx_context *)plugin;
    return context != NULL && context->magic == TX_MAGIC;
}

static fw_string_view tx_view(const char *text) {
    fw_string_view value;
    value.data = text;
    value.length = strlen(text);
    return value;
}

static int tx_string_shape(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int tx_string_equal(fw_string_view value, const char *literal) {
    const size_t length = strlen(literal);
    return value.length == length &&
        (length == 0u || memcmp(value.data, literal, length) == 0);
}

static int tx_utf8_valid(fw_string_view value) {
    size_t i = 0u;
    while (i < value.length) {
        const unsigned char first = (unsigned char)value.data[i++];
        uint32_t cp;
        size_t remaining;
        if (first < 0x80u) {
            continue;
        }
        if (first >= 0xc2u && first <= 0xdfu) {
            cp = first & 0x1fu;
            remaining = 1u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            cp = first & 0x0fu;
            remaining = 2u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            cp = first & 0x07u;
            remaining = 3u;
        } else {
            return 0;
        }
        if (remaining > value.length - i) {
            return 0;
        }
        while (remaining-- != 0u) {
            const unsigned char next = (unsigned char)value.data[i++];
            if ((next & 0xc0u) != 0x80u) {
                return 0;
            }
            cp = (cp << 6u) | (next & 0x3fu);
        }
        if ((cp >= 0xd800u && cp <= 0xdfffu) || cp > 0x10ffffu ||
            (cp < 0x800u && first >= 0xe0u) ||
            (cp < 0x10000u && first >= 0xf0u)) {
            return 0;
        }
    }
    return 1;
}

static int tx_finite_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
}

static int tx_color_valid(fw_color_rgba_f32 color) {
    return isfinite(color.red) && isfinite(color.green) &&
        isfinite(color.blue) && isfinite(color.alpha) &&
        color.red >= 0.0f && color.red <= 1.0f &&
        color.green >= 0.0f && color.green <= 1.0f &&
        color.blue >= 0.0f && color.blue <= 1.0f &&
        color.alpha >= 0.0f && color.alpha <= 1.0f;
}

static fw_color_rgba_f32 tx_apply_opacity(
    fw_color_rgba_f32 color,
    float opacity) {
    color.alpha *= opacity;
    return color;
}

static float tx_clamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int tx_direction_valid(fw_text_direction value) {
    return value <= FW_TEXT_DIRECTION_RTL;
}

static int tx_align_valid(fw_text_horizontal_align value) {
    return value <= FW_TEXT_ALIGN_JUSTIFY;
}

static fw_status tx_validate_request(
    fw_plugin_handle plugin,
    const fw_text_renderer_request_v1 *request,
    fw_text_normalization_flags *out_flags,
    const char **out_key) {
    size_t index;
    *out_flags = FW_TX_NORMALIZED_NONE;
    *out_key = "text.invalid_argument";
    if (!tx_context_valid(plugin) || request == NULL ||
        request->struct_size < sizeof(*request) ||
        request->style.struct_size < sizeof(request->style) ||
        request->layout.struct_size < sizeof(request->layout) ||
        request->constraints.struct_size < sizeof(request->constraints) ||
        request->target.struct_size < sizeof(request->target) ||
        request->session.struct_size < sizeof(request->session)) {
        *out_key = "text.invalid_struct_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!tx_string_shape(request->zone_id) ||
        !tx_string_shape(request->text) ||
        !tx_string_shape(request->language) ||
        !tx_string_shape(request->style.font_resource_id) ||
        request->text.length > TX_MAX_TEXT_BYTES ||
        request->style.font_family_count > 16u ||
        (request->style.font_family_count != 0u &&
         request->style.font_families == NULL)) {
        *out_key = request->text.length > TX_MAX_TEXT_BYTES ?
            "text.resource_limit" : "text.invalid_string";
        return request->text.length > TX_MAX_TEXT_BYTES ?
            FW_STATUS_RESOURCE_LIMIT : FW_STATUS_INVALID_ARGUMENT;
    }
    if (!tx_utf8_valid(request->zone_id) ||
        !tx_utf8_valid(request->text) ||
        !tx_utf8_valid(request->language) ||
        !tx_utf8_valid(request->style.font_resource_id)) {
        *out_key = "text.invalid_utf8";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < request->style.font_family_count; ++index) {
        if (!tx_string_shape(request->style.font_families[index]) ||
            !tx_utf8_valid(request->style.font_families[index])) {
            *out_key = "text.invalid_font_family";
            return FW_STATUS_INVALID_ARGUMENT;
        }
    }
    if (!tx_direction_valid(request->direction) ||
        !tx_align_valid(request->layout.horizontal_align) ||
        request->layout.vertical_align > FW_TEXT_ALIGN_BOTTOM ||
        request->layout.wrap > FW_TEXT_NO_WRAP ||
        request->layout.overflow > FW_TEXT_OVERFLOW_SCROLL ||
        request->style.font_style > FW_TEXT_FONT_OBLIQUE ||
        request->selectable > 1u || request->style.has_background_color > 1u) {
        *out_key = "text.invalid_enum";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->opacity) || request->opacity < 0.0f ||
        request->opacity > 1.0f || !isfinite(request->style.font_size) ||
        request->style.font_size <= 0.0f || request->style.font_weight < 1u ||
        request->style.font_weight > 1000u ||
        !isfinite(request->style.line_height_multiplier) ||
        request->style.line_height_multiplier < 0.5f ||
        !isfinite(request->style.letter_spacing) ||
        !tx_color_valid(request->style.color) ||
        (request->style.has_background_color != 0u &&
         !tx_color_valid(request->style.background_color))) {
        *out_key = "text.invalid_style";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!tx_finite_nonnegative(request->layout.padding.left) ||
        !tx_finite_nonnegative(request->layout.padding.top) ||
        !tx_finite_nonnegative(request->layout.padding.right) ||
        !tx_finite_nonnegative(request->layout.padding.bottom) ||
        !tx_finite_nonnegative(request->constraints.min_width) ||
        !tx_finite_nonnegative(request->constraints.max_width) ||
        !tx_finite_nonnegative(request->constraints.min_height) ||
        !tx_finite_nonnegative(request->constraints.max_height) ||
        request->constraints.max_width < request->constraints.min_width ||
        request->constraints.max_height < request->constraints.min_height) {
        *out_key = "text.invalid_constraints";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->target.font_scale) ||
        request->target.font_scale <= 0.0f) {
        *out_flags |= FW_TX_NORMALIZED_FONT_SCALE;
    }
    if (!isfinite(request->session.scroll_offset_y) ||
        request->session.scroll_offset_y < 0.0f) {
        *out_flags |= FW_TX_NORMALIZED_SCROLL_OFFSET;
    }
    for (index = 0u; index < request->text.length; ++index) {
        if (request->text.data[index] == '\r') {
            *out_flags |= FW_TX_NORMALIZED_NEWLINES;
            break;
        }
    }
    *out_key = *out_flags == 0u ? "text.valid" :
        "text.valid_with_normalization";
    return FW_STATUS_OK;
}

static fw_status tx_normalize_text(
    fw_string_view input,
    char **out_owned,
    fw_string_view *out_text) {
    size_t read_index;
    size_t write_index = 0u;
    char *buffer = NULL;
    *out_owned = NULL;
    *out_text = input;
    for (read_index = 0u; read_index < input.length; ++read_index) {
        if (input.data[read_index] == '\r') {
            buffer = (char *)malloc(input.length == 0u ? 1u : input.length);
            if (buffer == NULL) return FW_STATUS_OUT_OF_MEMORY;
            break;
        }
    }
    if (buffer == NULL) return FW_STATUS_OK;
    for (read_index = 0u; read_index < input.length; ++read_index) {
        const char value = input.data[read_index];
        if (value == '\r') {
            if (read_index + 1u < input.length &&
                input.data[read_index + 1u] == '\n') {
                ++read_index;
            }
            buffer[write_index++] = '\n';
        } else {
            buffer[write_index++] = value;
        }
    }
    out_text->data = buffer;
    out_text->length = write_index;
    *out_owned = buffer;
    return FW_STATUS_OK;
}

static fw_status tx_layout(
    const fw_text_renderer_request_v1 *request,
    const fw_text_service_v2 *service,
    float max_width,
    float max_height,
    fw_text_layout_handle *out_handle,
    fw_text_layout_metrics_v2 *out_metrics,
    char **out_owned_text) {
    fw_text_layout_request_v2 layout_request;
    fw_string_view normalized_text;
    fw_status status;
    if (service == NULL || service->struct_size < sizeof(*service) ||
        service->layout_utf8_v2 == NULL || service->release_layout == NULL ||
        out_handle == NULL || out_metrics == NULL || out_owned_text == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_handle = NULL;
    *out_owned_text = NULL;
    memset(out_metrics, 0, sizeof(*out_metrics));
    out_metrics->struct_size = sizeof(*out_metrics);
    status = tx_normalize_text(request->text, out_owned_text, &normalized_text);
    if (status != FW_STATUS_OK) return status;
    memset(&layout_request, 0, sizeof(layout_request));
    layout_request.struct_size = sizeof(layout_request);
    layout_request.text = normalized_text;
    layout_request.language = request->language;
    layout_request.font_families = request->style.font_families;
    layout_request.font_family_count = request->style.font_family_count;
    layout_request.font_resource_id = request->style.font_resource_id;
    layout_request.font_size = request->style.font_size *
        ((isfinite(request->target.font_scale) &&
          request->target.font_scale > 0.0f) ? request->target.font_scale : 1.0f);
    layout_request.font_weight = request->style.font_weight;
    layout_request.font_style = request->style.font_style;
    layout_request.line_height_multiplier =
        request->style.line_height_multiplier;
    layout_request.letter_spacing = request->style.letter_spacing;
    layout_request.max_width = max_width;
    layout_request.max_height = max_height;
    layout_request.max_lines = request->layout.max_lines;
    if (request->layout.wrap == FW_TEXT_NO_WRAP &&
        request->layout.overflow == FW_TEXT_OVERFLOW_ELLIPSIS) {
        layout_request.max_lines = 1u;
    }
    layout_request.direction = request->direction;
    layout_request.horizontal_align = request->layout.horizontal_align;
    layout_request.wrap = request->layout.wrap;
    layout_request.ellipsize =
        request->layout.overflow == FW_TEXT_OVERFLOW_ELLIPSIS;
    layout_request.decorations = request->style.decorations;
    status = service->layout_utf8_v2(
        service->user_data, &layout_request, out_handle, out_metrics);
    if (status != FW_STATUS_OK || *out_handle == NULL ||
        !tx_finite_nonnegative(out_metrics->size.width) ||
        !tx_finite_nonnegative(out_metrics->size.height) ||
        !isfinite(out_metrics->first_baseline)) {
        if (*out_handle != NULL) service->release_layout(service->user_data, *out_handle);
        *out_handle = NULL;
        free(*out_owned_text);
        *out_owned_text = NULL;
        return status == FW_STATUS_OK ? FW_STATUS_PLUGIN_ERROR : status;
    }
    return FW_STATUS_OK;
}

static void tx_release_layout(
    const fw_text_service_v2 *service,
    fw_text_layout_handle handle,
    char *owned_text) {
    if (handle != NULL) service->release_layout(service->user_data, handle);
    free(owned_text);
}

static fw_status FW_CALL tx_validate(
    fw_plugin_handle plugin,
    const fw_text_renderer_request_v1 *request,
    fw_text_validation_result_v1 *out_result) {
    fw_text_normalization_flags flags;
    const char *key;
    fw_status status;
    uint32_t output_size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = tx_validate_request(plugin, request, &flags, &key);
    out_result->status = status;
    out_result->normalization_flags = flags;
    out_result->diagnostic_key = tx_view(key);
    return status;
}

static fw_status FW_CALL tx_measure(
    fw_plugin_handle plugin,
    const fw_text_renderer_request_v1 *request,
    const fw_text_services_v1 *services,
    fw_text_measure_result_v1 *out_result) {
    fw_text_normalization_flags flags;
    const char *key;
    fw_text_layout_handle handle;
    fw_text_layout_metrics_v2 metrics;
    char *owned_text;
    float horizontal;
    float vertical;
    float content_width;
    float content_height;
    fw_status status;
    uint32_t output_size;
    (void)key;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        services == NULL || services->struct_size < sizeof(*services)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = tx_validate_request(plugin, request, &flags, &key);
    if (status != FW_STATUS_OK) return status;
    horizontal = request->layout.padding.left + request->layout.padding.right;
    vertical = request->layout.padding.top + request->layout.padding.bottom;
    content_width = request->constraints.max_width > horizontal ?
        request->constraints.max_width - horizontal : 0.0f;
    content_height = request->constraints.max_height > vertical ?
        request->constraints.max_height - vertical : 0.0f;
    status = tx_layout(request, services->text, content_width, content_height,
        &handle, &metrics, &owned_text);
    if (status != FW_STATUS_OK) return status;
    out_result->content_extent = metrics.size;
    out_result->viewport_extent.width = content_width;
    out_result->viewport_extent.height =
        request->layout.overflow == FW_TEXT_OVERFLOW_SCROLL ?
        content_height : metrics.size.height;
    out_result->size.width = tx_clamp(metrics.size.width + horizontal,
        request->constraints.min_width, request->constraints.max_width);
    out_result->size.height = tx_clamp(
        out_result->viewport_extent.height + vertical,
        request->constraints.min_height, request->constraints.max_height);
    out_result->first_baseline =
        request->layout.padding.top + metrics.first_baseline;
    out_result->line_count = metrics.line_count;
    out_result->normalization_flags = flags;
    if ((metrics.flags & FW_TEXT_LAYOUT_FONT_FALLBACK) != 0u)
        out_result->normalization_flags |= FW_TX_FONT_FALLBACK;
    if ((metrics.flags & FW_TEXT_LAYOUT_DID_TRUNCATE) != 0u)
        out_result->normalization_flags |= FW_TX_VISUALLY_TRUNCATED;
    tx_release_layout(services->text, handle, owned_text);
    return FW_STATUS_OK;
}

static void tx_hash_bytes(uint64_t *high, uint64_t *low,
    const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0u; i < length; ++i) {
        *low ^= bytes[i];
        *low *= UINT64_C(1099511628211);
        *high ^= (*low >> 24u) ^ bytes[i];
        *high *= UINT64_C(14029467366897019727);
    }
}

static fw_status FW_CALL tx_render(
    fw_plugin_handle plugin,
    const fw_text_renderer_request_v1 *request,
    fw_rect_f32 bounds,
    const fw_text_services_v1 *services,
    fw_text_render_result_v1 *out_result) {
    fw_text_normalization_flags flags;
    const char *key;
    fw_text_layout_handle handle;
    fw_text_layout_metrics_v2 metrics;
    char *owned_text;
    fw_rect_f32 content;
    fw_point_f32 origin;
    float scroll;
    float max_scroll;
    fw_status status;
    fw_status first_error = FW_STATUS_OK;
    uint32_t saved = 0u;
    uint32_t output_size;
    uint64_t high = UINT64_C(7809847782465536322);
    uint64_t low = UINT64_C(1469598103934665603);
    const fw_display_list_sink_v1 *sink;
    fw_text_style_v1 style_key;
    size_t family_index;
    (void)key;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        services == NULL || services->struct_size < sizeof(*services) ||
        services->display_list == NULL ||
        services->display_list->struct_size < sizeof(*services->display_list) ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !tx_finite_nonnegative(bounds.width) ||
        !tx_finite_nonnegative(bounds.height)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = tx_validate_request(plugin, request, &flags, &key);
    if (status != FW_STATUS_OK) return status;
    sink = services->display_list;
    if (sink->save == NULL || sink->restore == NULL ||
        sink->clip_rect == NULL || sink->fill_rounded_rect == NULL ||
        sink->draw_text_layout == NULL) return FW_STATUS_INVALID_ARGUMENT;
    content.x = bounds.x + request->layout.padding.left;
    content.y = bounds.y + request->layout.padding.top;
    content.width = bounds.width - request->layout.padding.left -
        request->layout.padding.right;
    content.height = bounds.height - request->layout.padding.top -
        request->layout.padding.bottom;
    if (content.width < 0.0f) content.width = 0.0f;
    if (content.height < 0.0f) content.height = 0.0f;
    status = tx_layout(request, services->text, content.width, content.height,
        &handle, &metrics, &owned_text);
    if (status != FW_STATUS_OK) return status;
    max_scroll = metrics.size.height > content.height ?
        metrics.size.height - content.height : 0.0f;
    scroll = (isfinite(request->session.scroll_offset_y) &&
        request->session.scroll_offset_y >= 0.0f) ?
        request->session.scroll_offset_y : 0.0f;
    scroll = tx_clamp(scroll, 0.0f, max_scroll);
    origin.x = content.x;
    if (request->layout.horizontal_align == FW_TEXT_ALIGN_CENTER)
        origin.x += (content.width - metrics.size.width) * 0.5f;
    else if (request->layout.horizontal_align == FW_TEXT_ALIGN_END)
        origin.x += request->direction == FW_TEXT_DIRECTION_RTL ? 0.0f :
            content.width - metrics.size.width;
    else if (request->layout.horizontal_align == FW_TEXT_ALIGN_START &&
        request->direction == FW_TEXT_DIRECTION_RTL)
        origin.x += content.width - metrics.size.width;
    origin.y = content.y - scroll;
    if (request->layout.overflow != FW_TEXT_OVERFLOW_SCROLL) {
        if (request->layout.vertical_align == FW_TEXT_ALIGN_MIDDLE)
            origin.y += (content.height - metrics.size.height) * 0.5f;
        else if (request->layout.vertical_align == FW_TEXT_ALIGN_BOTTOM)
            origin.y += content.height - metrics.size.height;
    }
    if (request->opacity > 0.0f) {
        first_error = sink->save(sink->user_data);
        if (first_error == FW_STATUS_OK) {
            saved = 1u;
            ++out_result->emitted_command_count;
        }
        if (first_error == FW_STATUS_OK &&
            request->layout.overflow != FW_TEXT_OVERFLOW_VISIBLE) {
            first_error = sink->clip_rect(sink->user_data, content);
            if (first_error == FW_STATUS_OK) ++out_result->emitted_command_count;
        }
        if (first_error == FW_STATUS_OK &&
            request->style.has_background_color != 0u &&
            request->style.background_color.alpha * request->opacity > 0.0f) {
            first_error = sink->fill_rounded_rect(sink->user_data, content,
                0.0f, tx_apply_opacity(
                    request->style.background_color, request->opacity));
            if (first_error == FW_STATUS_OK) ++out_result->emitted_command_count;
        }
        if (first_error == FW_STATUS_OK &&
            request->style.color.alpha * request->opacity > 0.0f) {
            first_error = sink->draw_text_layout(sink->user_data, handle,
                origin, tx_apply_opacity(request->style.color, request->opacity));
            if (first_error == FW_STATUS_OK) ++out_result->emitted_command_count;
        }
        if (saved != 0u) {
            const fw_status restore_status = sink->restore(sink->user_data);
            if (restore_status == FW_STATUS_OK)
                ++out_result->emitted_command_count;
            else if (first_error == FW_STATUS_OK)
                first_error = restore_status;
        }
    }
    out_result->content_extent = metrics.size;
    out_result->viewport_extent =
        (fw_size_f32){content.width, content.height};
    out_result->applied_scroll_offset_y = scroll;
    out_result->max_scroll_offset_y = max_scroll;
    out_result->normalization_flags = flags;
    if ((metrics.flags & FW_TEXT_LAYOUT_FONT_FALLBACK) != 0u)
        out_result->normalization_flags |= FW_TX_FONT_FALLBACK;
    if ((metrics.flags & FW_TEXT_LAYOUT_DID_TRUNCATE) != 0u)
        out_result->normalization_flags |= FW_TX_VISUALLY_TRUNCATED;
    style_key = request->style;
    style_key.font_families = NULL;
    style_key.font_resource_id.data = NULL;
    tx_hash_bytes(&high, &low, request->text.data, request->text.length);
    tx_hash_bytes(&high, &low, request->language.data, request->language.length);
    tx_hash_bytes(&high, &low, &style_key, sizeof(style_key));
    tx_hash_bytes(&high, &low, request->style.font_resource_id.data,
        request->style.font_resource_id.length);
    for (family_index = 0u;
        family_index < request->style.font_family_count; ++family_index) {
        tx_hash_bytes(&high, &low,
            &request->style.font_families[family_index].length,
            sizeof(request->style.font_families[family_index].length));
        tx_hash_bytes(&high, &low,
            request->style.font_families[family_index].data,
            request->style.font_families[family_index].length);
    }
    tx_hash_bytes(&high, &low, &request->layout, sizeof(request->layout));
    tx_hash_bytes(&high, &low, &bounds, sizeof(bounds));
    tx_hash_bytes(&high, &low, &request->session.presentation_revision,
        sizeof(request->session.presentation_revision));
    tx_hash_bytes(&high, &low, &scroll, sizeof(scroll));
    tx_hash_bytes(&high, &low, metrics.resolved_font_key.data,
        metrics.resolved_font_key.length);
    out_result->cache_key_high = high;
    out_result->cache_key_low = low;
    tx_release_layout(services->text, handle, owned_text);
    return first_error == FW_STATUS_OK ? FW_STATUS_OK : FW_STATUS_SINK_REJECTED;
}

static fw_status FW_CALL tx_build_semantics(
    fw_plugin_handle plugin,
    const fw_text_renderer_request_v1 *request,
    fw_rect_f32 bounds,
    const fw_text_measure_result_v1 *measurement,
    fw_text_semantics_v1 *out_semantics) {
    fw_text_normalization_flags flags;
    const char *key;
    fw_status status;
    uint32_t output_size;
    (void)key;
    if (measurement == NULL || measurement->struct_size < sizeof(*measurement) ||
        out_semantics == NULL || out_semantics->struct_size < sizeof(*out_semantics) ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !tx_finite_nonnegative(bounds.width) ||
        !tx_finite_nonnegative(bounds.height)) return FW_STATUS_INVALID_ARGUMENT;
    output_size = out_semantics->struct_size;
    memset(out_semantics, 0, sizeof(*out_semantics));
    out_semantics->struct_size = output_size;
    status = tx_validate_request(plugin, request, &flags, &key);
    if (status != FW_STATUS_OK) return status;
    out_semantics->role = FW_SEMANTICS_ROLE_TEXT;
    out_semantics->text = request->text;
    out_semantics->language = request->language;
    out_semantics->direction = request->direction;
    out_semantics->bounds = bounds;
    out_semantics->selectable = request->selectable;
    out_semantics->scrollable =
        request->layout.overflow == FW_TEXT_OVERFLOW_SCROLL;
    out_semantics->visually_truncated =
        (measurement->normalization_flags & FW_TX_VISUALLY_TRUNCATED) != 0u;
    out_semantics->hidden = request->session.hidden_from_semantics != 0u;
    out_semantics->scroll_offset_y = request->session.scroll_offset_y;
    out_semantics->max_scroll_offset_y =
        measurement->content_extent.height > measurement->viewport_extent.height ?
        measurement->content_extent.height - measurement->viewport_extent.height : 0.0f;
    return FW_STATUS_OK;
}

static fw_status FW_CALL tx_get_parameter_schema(
    fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!tx_context_valid(plugin) || out_schema_json == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    out_schema_json->data = tx_parameter_schema;
    out_schema_json->length = sizeof(tx_parameter_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_text_renderer_api_v1 tx_renderer_api = {
    sizeof(fw_text_renderer_api_v1),
    FW_TEXT_RENDERER_INTERFACE_VERSION,
    tx_validate,
    tx_measure,
    tx_render,
    tx_build_semantics,
    tx_get_parameter_schema,
};

static const fw_plugin_descriptor_v1 *FW_CALL tx_get_descriptor(void) {
    return &tx_descriptor;
}

static fw_status FW_CALL tx_load(
    const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    tx_context *context;
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL ||
        host->struct_size < sizeof(*host) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR)
        return FW_STATUS_INVALID_ARGUMENT;
    context = (tx_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FW_STATUS_OUT_OF_MEMORY;
    context->magic = TX_MAGIC;
    context->host = *host;
    *out_handle = context;
    return FW_STATUS_OK;
}

static void FW_CALL tx_unload(fw_plugin_handle handle) {
    tx_context *context = (tx_context *)handle;
    if (context != NULL && context->magic == TX_MAGIC) {
        context->magic = 0u;
        free(context);
    }
}

static fw_status FW_CALL tx_query_interface(
    fw_plugin_handle handle,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (!tx_context_valid(handle) || !tx_string_shape(interface_id))
        return FW_STATUS_INVALID_ARGUMENT;
    if (!tx_string_equal(interface_id, FW_TEXT_RENDERER_INTERFACE_ID) ||
        minimum_version > FW_TEXT_RENDERER_INTERFACE_VERSION)
        return FW_STATUS_NOT_FOUND;
    *out_interface = &tx_renderer_api;
    return FW_STATUS_OK;
}

static const fw_plugin_api_v1 tx_plugin_api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    tx_get_descriptor, tx_load, tx_unload, tx_query_interface,
};

#if defined(FACETWIRE_TEXT_RENDERER_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_text_renderer_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) return NULL;
    return &tx_plugin_api;
}

#if defined(FACETWIRE_TEXT_RENDERER_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    return facetwire_text_renderer_plugin_query(requested_abi);
}
#endif
