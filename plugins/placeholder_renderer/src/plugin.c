/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/placeholder_renderer.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PH_CONTEXT_MAGIC 0x50485231u
#define PH_MAX_STRING_BYTES 4096u

typedef struct ph_context {
    uint32_t magic;
    fw_host_api_v1 host;
} ph_context;

static const fw_capability_descriptor_v1 ph_capabilities[] = {
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL(FW_PLACEHOLDER_RENDERER_CAPABILITY_ID),
        FW_STRING_VIEW_LITERAL(FW_RENDERER_CAPABILITY_KIND),
        FW_RENDERER_FLAG_DETERMINISTIC | FW_RENDERER_FLAG_HEADLESS |
            FW_RENDERER_FLAG_SEMANTICS | FW_RENDERER_FLAG_HIT_TEST,
    },
};

static const fw_plugin_descriptor_v1 ph_descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.placeholder-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Placeholder Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    ph_capabilities,
    sizeof(ph_capabilities) / sizeof(ph_capabilities[0]),
};

static const char ph_parameter_schema[] =
    "{\"schemaVersion\":1,\"parameters\":["
    "{\"id\":\"mode\",\"type\":\"enum\",\"default\":\"standard\","
    "\"values\":[\"hidden\",\"minimal\",\"standard\",\"diagnostic\"]},"
    "{\"id\":\"opacity\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0,\"maximum\":1,\"step\":0.05},"
    "{\"id\":\"borderWidth\",\"type\":\"number\",\"default\":1,"
    "\"minimum\":0},"
    "{\"id\":\"cornerRadius\",\"type\":\"number\",\"default\":8,"
    "\"minimum\":0},"
    "{\"id\":\"showDiagnosticCode\",\"type\":\"boolean\",\"default\":false},"
    "{\"id\":\"animationEnabled\",\"type\":\"boolean\",\"default\":true}]}";

static int ph_context_is_valid(fw_plugin_handle plugin) {
    const ph_context *context = (const ph_context *)plugin;
    return context != NULL && context->magic == PH_CONTEXT_MAGIC;
}

static int ph_string_equal(fw_string_view left, const char *right) {
    const size_t length = strlen(right);
    return left.length == length &&
        (length == 0u || memcmp(left.data, right, length) == 0);
}

static int ph_string_shape_is_valid(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int ph_utf8_is_valid(fw_string_view value) {
    size_t index = 0u;
    while (index < value.length) {
        const unsigned char first = (unsigned char)value.data[index++];
        uint32_t codepoint;
        size_t remaining;
        if (first < 0x80u) {
            continue;
        }
        if (first >= 0xc2u && first <= 0xdfu) {
            codepoint = first & 0x1fu;
            remaining = 1u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            codepoint = first & 0x0fu;
            remaining = 2u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            codepoint = first & 0x07u;
            remaining = 3u;
        } else {
            return 0;
        }
        if (remaining > value.length - index) {
            return 0;
        }
        while (remaining-- > 0u) {
            const unsigned char next = (unsigned char)value.data[index++];
            if ((next & 0xc0u) != 0x80u) {
                return 0;
            }
            codepoint = (codepoint << 6u) | (next & 0x3fu);
        }
        if ((codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
            codepoint > 0x10ffffu ||
            (codepoint < 0x800u && first >= 0xe0u) ||
            (codepoint < 0x10000u && first >= 0xf0u)) {
            return 0;
        }
    }
    return 1;
}

static fw_string_view ph_static_view(const char *value) {
    fw_string_view view;
    view.data = value;
    view.length = strlen(value);
    return view;
}

static int ph_is_finite_nonnegative(float value) {
    return isfinite(value) && value >= 0.0f;
}

static int ph_color_needs_normalization(fw_color_rgba_f32 color) {
    return !isfinite(color.red) || !isfinite(color.green) ||
        !isfinite(color.blue) || !isfinite(color.alpha) ||
        color.red < 0.0f || color.red > 1.0f ||
        color.green < 0.0f || color.green > 1.0f ||
        color.blue < 0.0f || color.blue > 1.0f ||
        color.alpha < 0.0f || color.alpha > 1.0f;
}

static float ph_clamp_unit(float value) {
    if (!isfinite(value)) {
        return 0.0f;
    }
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value == 0.0f ? 0.0f : value;
}

static fw_color_rgba_f32 ph_normalize_color(
    fw_color_rgba_f32 color,
    float opacity) {
    color.red = ph_clamp_unit(color.red);
    color.green = ph_clamp_unit(color.green);
    color.blue = ph_clamp_unit(color.blue);
    color.alpha = ph_clamp_unit(color.alpha) * opacity;
    return color;
}

static int ph_reason_is_known(fw_placeholder_reason reason) {
    return reason >= FW_PLACEHOLDER_REASON_LOADING &&
        reason <= FW_PLACEHOLDER_REASON_UNKNOWN;
}

static int ph_mode_is_known(fw_placeholder_mode mode) {
    return mode >= FW_PLACEHOLDER_MODE_HIDDEN &&
        mode <= FW_PLACEHOLDER_MODE_DIAGNOSTIC;
}

static fw_status ph_validate_core(
    fw_plugin_handle plugin,
    const fw_placeholder_request_v1 *request,
    fw_placeholder_normalization_flags *out_flags,
    const char **out_diagnostic) {
    const fw_string_view strings[] = {
        request == NULL ? (fw_string_view){NULL, 0u} : request->zone_id,
        request == NULL ? (fw_string_view){NULL, 0u} : request->content_kind,
        request == NULL ? (fw_string_view){NULL, 0u} : request->required_capability_id,
        request == NULL ? (fw_string_view){NULL, 0u} : request->accessible_label,
        request == NULL ? (fw_string_view){NULL, 0u} : request->diagnostic_code,
    };
    fw_placeholder_normalization_flags flags = FW_PH_NORMALIZED_NONE;
    size_t index;
    uint32_t fragment_count;

    *out_flags = FW_PH_NORMALIZED_NONE;
    *out_diagnostic = "placeholder.invalid_argument";
    if (!ph_context_is_valid(plugin) || request == NULL ||
        request->struct_size < sizeof(fw_placeholder_request_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->constraints.struct_size < sizeof(fw_layout_constraints_v1) ||
        request->style.struct_size < sizeof(fw_placeholder_style_v1) ||
        request->target.struct_size < sizeof(fw_render_target_profile_v1) ||
        request->progress.struct_size < sizeof(fw_placeholder_progress_v1)) {
        *out_diagnostic = "placeholder.invalid_struct_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < sizeof(strings) / sizeof(strings[0]); ++index) {
        if (!ph_string_shape_is_valid(strings[index])) {
            *out_diagnostic = "placeholder.invalid_string";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        if (strings[index].length > PH_MAX_STRING_BYTES) {
            *out_diagnostic = "placeholder.string_too_long";
            return FW_STATUS_RESOURCE_LIMIT;
        }
        if (!ph_utf8_is_valid(strings[index])) {
            *out_diagnostic = "placeholder.invalid_utf8";
            return FW_STATUS_INVALID_ARGUMENT;
        }
    }
    if (!isfinite(request->constraints.min_width) ||
        !isfinite(request->constraints.max_width) ||
        !isfinite(request->constraints.min_height) ||
        !isfinite(request->constraints.max_height) ||
        request->constraints.min_width < 0.0f ||
        request->constraints.min_height < 0.0f ||
        request->constraints.max_width < request->constraints.min_width ||
        request->constraints.max_height < request->constraints.min_height) {
        *out_diagnostic = "placeholder.invalid_constraints";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->constraints.em_size) ||
        request->constraints.em_size <= 0.0f ||
        !isfinite(request->constraints.line_height) ||
        request->constraints.line_height <= 0.0f) {
        flags |= FW_PH_NORMALIZED_CONSTRAINTS;
    }
    if (request->resolved_size.has_value > 1u ||
        request->intrinsic_size.has_value > 1u ||
        request->intrinsic_aspect_ratio.has_value > 1u) {
        flags |= FW_PH_NORMALIZED_INTRINSIC;
    }
    if (request->resolved_size.has_value == 1u &&
        (!ph_is_finite_nonnegative(request->resolved_size.value.width) ||
         !ph_is_finite_nonnegative(request->resolved_size.value.height))) {
        *out_diagnostic = "placeholder.invalid_resolved_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->intrinsic_size.has_value == 1u &&
        (!ph_is_finite_nonnegative(request->intrinsic_size.value.width) ||
         !ph_is_finite_nonnegative(request->intrinsic_size.value.height))) {
        *out_diagnostic = "placeholder.invalid_intrinsic_size";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->intrinsic_aspect_ratio.has_value == 1u &&
        (!isfinite(request->intrinsic_aspect_ratio.value) ||
         request->intrinsic_aspect_ratio.value <= 0.0f)) {
        *out_diagnostic = "placeholder.invalid_aspect_ratio";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!ph_is_finite_nonnegative(request->style.border_width) ||
        !ph_is_finite_nonnegative(request->style.corner_radius) ||
        !ph_is_finite_nonnegative(request->style.content_padding) ||
        !ph_is_finite_nonnegative(request->style.gap) ||
        !ph_is_finite_nonnegative(request->style.icon_size)) {
        *out_diagnostic = "placeholder.invalid_style_geometry";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!isfinite(request->style.opacity) || request->style.opacity < 0.0f ||
        request->style.opacity > 1.0f ||
        ph_color_needs_normalization(request->style.background) ||
        ph_color_needs_normalization(request->style.border) ||
        ph_color_needs_normalization(request->style.icon) ||
        ph_color_needs_normalization(request->style.primary_text) ||
        ph_color_needs_normalization(request->style.secondary_text) ||
        ph_color_needs_normalization(request->style.action)) {
        flags |= FW_PH_NORMALIZED_STYLE;
    }
    if (!isfinite(request->target.device_pixel_ratio) ||
        request->target.device_pixel_ratio <= 0.0f ||
        !isfinite(request->target.font_scale) ||
        request->target.font_scale <= 0.0f ||
        request->target.medium < FW_RENDER_MEDIUM_SCREEN ||
        request->target.medium > FW_RENDER_MEDIUM_HEADLESS) {
        flags |= FW_PH_NORMALIZED_STYLE;
    }
    if (!ph_reason_is_known(request->reason)) {
        flags |= FW_PH_NORMALIZED_REASON;
    }
    if (!ph_mode_is_known(request->mode)) {
        flags |= FW_PH_NORMALIZED_MODE;
    }
    if ((request->permitted_actions & ~FW_PLACEHOLDER_ACTION_ALL) != 0u) {
        flags |= FW_PH_NORMALIZED_ACTIONS;
    }
    fragment_count = request->fragment_count == 0u ? 1u : request->fragment_count;
    if (request->fragment_count == 0u) {
        flags |= FW_PH_NORMALIZED_CONSTRAINTS;
    }
    if (request->fragment_index >= fragment_count) {
        *out_diagnostic = "placeholder.invalid_fragment";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->phase > FW_PLACEHOLDER_PHASE_READY_FOR_HANDOFF ||
        request->progress.kind > FW_PLACEHOLDER_PROGRESS_FRACTION ||
        request->stale > 1u) {
        flags |= FW_PH_NORMALIZED_AVAILABILITY;
    }
    if (request->progress.kind == FW_PLACEHOLDER_PROGRESS_FRACTION &&
        (request->progress.total == 0u ||
         request->progress.completed > request->progress.total)) {
        *out_diagnostic = "placeholder.invalid_progress";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_flags = flags;
    *out_diagnostic = flags == 0u ? "placeholder.valid" :
        "placeholder.valid_with_normalization";
    return FW_STATUS_OK;
}

static void ph_normalize_request(
    const fw_placeholder_request_v1 *input,
    fw_placeholder_request_v1 *output) {
    *output = *input;
    if (!ph_reason_is_known(output->reason)) {
        output->reason = FW_PLACEHOLDER_REASON_UNKNOWN;
    }
    if (!ph_mode_is_known(output->mode)) {
        output->mode = FW_PLACEHOLDER_MODE_STANDARD;
    }
    output->permitted_actions &= FW_PLACEHOLDER_ACTION_ALL;
    if (!isfinite(output->constraints.em_size) ||
        output->constraints.em_size <= 0.0f) {
        output->constraints.em_size = 16.0f;
    }
    if (!isfinite(output->constraints.line_height) ||
        output->constraints.line_height <= 0.0f) {
        output->constraints.line_height = output->constraints.em_size * 1.2f;
    }
    if (output->resolved_size.has_value != 1u) {
        output->resolved_size.has_value = 0u;
    }
    if (output->intrinsic_size.has_value != 1u) {
        output->intrinsic_size.has_value = 0u;
    }
    if (output->intrinsic_aspect_ratio.has_value != 1u) {
        output->intrinsic_aspect_ratio.has_value = 0u;
    }
    output->style.opacity = isfinite(output->style.opacity) ?
        ph_clamp_unit(output->style.opacity) : 1.0f;
    if (!isfinite(output->target.device_pixel_ratio) ||
        output->target.device_pixel_ratio <= 0.0f) {
        output->target.device_pixel_ratio = 1.0f;
    }
    if (!isfinite(output->target.font_scale) ||
        output->target.font_scale <= 0.0f) {
        output->target.font_scale = 1.0f;
    }
    if (output->target.medium < FW_RENDER_MEDIUM_SCREEN ||
        output->target.medium > FW_RENDER_MEDIUM_HEADLESS) {
        output->target.medium = FW_RENDER_MEDIUM_SCREEN;
    }
    if (output->fragment_count == 0u) {
        output->fragment_count = 1u;
    }
    if (output->phase > FW_PLACEHOLDER_PHASE_READY_FOR_HANDOFF) {
        output->phase = FW_PLACEHOLDER_PHASE_NONE;
    }
    if (output->progress.kind > FW_PLACEHOLDER_PROGRESS_FRACTION) {
        output->progress.kind = FW_PLACEHOLDER_PROGRESS_NONE;
    }
    if (output->progress.kind != FW_PLACEHOLDER_PROGRESS_FRACTION) {
        output->progress.completed = 0u;
        output->progress.total = 0u;
    }
    output->stale = output->stale == 0u ? 0u : 1u;
}

static fw_size_f32 ph_constrain_size(
    fw_size_f32 size,
    const fw_layout_constraints_v1 *constraints) {
    if (size.width < constraints->min_width) {
        size.width = constraints->min_width;
    }
    if (size.width > constraints->max_width) {
        size.width = constraints->max_width;
    }
    if (size.height < constraints->min_height) {
        size.height = constraints->min_height;
    }
    if (size.height > constraints->max_height) {
        size.height = constraints->max_height;
    }
    if (size.width == 0.0f) {
        size.width = 0.0f;
    }
    if (size.height == 0.0f) {
        size.height = 0.0f;
    }
    return size;
}

static fw_status FW_CALL ph_validate(
    fw_plugin_handle plugin,
    const fw_placeholder_request_v1 *request,
    fw_placeholder_validation_result_v1 *out_result) {
    fw_placeholder_normalization_flags flags;
    const char *diagnostic;
    fw_status status;
    uint32_t output_size;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(fw_placeholder_validation_result_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = ph_validate_core(plugin, request, &flags, &diagnostic);
    out_result->status = status;
    out_result->normalization_flags = flags;
    out_result->diagnostic_key = ph_static_view(diagnostic);
    return status;
}

static fw_status FW_CALL ph_measure(
    fw_plugin_handle plugin,
    const fw_placeholder_request_v1 *request,
    fw_placeholder_measure_result_v1 *out_result) {
    fw_placeholder_normalization_flags flags;
    const char *diagnostic;
    fw_placeholder_request_v1 normalized;
    fw_size_f32 size = {0.0f, 0.0f};
    fw_placeholder_measure_source source = 0u;
    fw_status status;
    uint32_t output_size;
    int width_explicit;
    int height_explicit;
    (void)diagnostic;

    if (out_result == NULL ||
        out_result->struct_size < sizeof(fw_placeholder_measure_result_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = ph_validate_core(plugin, request, &flags, &diagnostic);
    out_result->normalization_flags = flags;
    if (status != FW_STATUS_OK) {
        return status;
    }
    ph_normalize_request(request, &normalized);
    width_explicit = normalized.constraints.min_width ==
        normalized.constraints.max_width;
    height_explicit = normalized.constraints.min_height ==
        normalized.constraints.max_height;
    if (normalized.resolved_size.has_value == 1u) {
        size = normalized.resolved_size.value;
        source = FW_PH_MEASURE_RESOLVED;
    } else if (width_explicit && height_explicit) {
        size.width = normalized.constraints.min_width;
        size.height = normalized.constraints.min_height;
        source = FW_PH_MEASURE_EXPLICIT_CONSTRAINT;
    } else if (width_explicit &&
        normalized.intrinsic_aspect_ratio.has_value == 1u) {
        size.width = normalized.constraints.min_width;
        size.height = size.width / normalized.intrinsic_aspect_ratio.value;
        source = FW_PH_MEASURE_WIDTH_AND_RATIO;
    } else if (height_explicit &&
        normalized.intrinsic_aspect_ratio.has_value == 1u) {
        size.height = normalized.constraints.min_height;
        size.width = size.height * normalized.intrinsic_aspect_ratio.value;
        source = FW_PH_MEASURE_HEIGHT_AND_RATIO;
    } else if (normalized.intrinsic_size.has_value == 1u) {
        size = normalized.intrinsic_size.value;
        source = FW_PH_MEASURE_INTRINSIC;
    } else {
        const float em = normalized.constraints.em_size;
        if (ph_string_equal(normalized.content_kind, "image") ||
            ph_string_equal(normalized.content_kind, "video") ||
            ph_string_equal(normalized.content_kind, "chart")) {
            size.width = 16.0f * em;
            size.height = 9.0f * em;
            source = FW_PH_MEASURE_KIND_FALLBACK;
        } else if (ph_string_equal(normalized.content_kind, "audio")) {
            size.width = 16.0f * em;
            size.height = 3.0f * em;
            source = FW_PH_MEASURE_KIND_FALLBACK;
        } else if (ph_string_equal(normalized.content_kind, "text")) {
            size.width = 16.0f * em;
            size.height = 6.0f * em;
            source = FW_PH_MEASURE_KIND_FALLBACK;
        } else {
            size.width = 16.0f * em;
            size.height = 9.0f * em;
            source = FW_PH_MEASURE_GENERIC_FALLBACK;
        }
    }
    out_result->size = ph_constrain_size(size, &normalized.constraints);
    out_result->source = source;
    return FW_STATUS_OK;
}

static fw_placeholder_action_mask ph_supported_actions(
    fw_placeholder_reason reason) {
    switch (reason) {
    case FW_PLACEHOLDER_REASON_RENDERER_MISSING:
    case FW_PLACEHOLDER_REASON_UNSUPPORTED_TYPE:
        return FW_PLACEHOLDER_ACTION_FIND_PLUGIN |
            FW_PLACEHOLDER_ACTION_ALTERNATIVE |
            FW_PLACEHOLDER_ACTION_SHOW_DETAILS;
    case FW_PLACEHOLDER_REASON_RESOURCE_MISSING:
        return FW_PLACEHOLDER_ACTION_LOCATE | FW_PLACEHOLDER_ACTION_RETRY |
            FW_PLACEHOLDER_ACTION_SHOW_DETAILS;
    case FW_PLACEHOLDER_REASON_PERMISSION_REQUIRED:
        return FW_PLACEHOLDER_ACTION_PERMISSION |
            FW_PLACEHOLDER_ACTION_SHOW_DETAILS;
    case FW_PLACEHOLDER_REASON_POLICY_BLOCKED:
        return FW_PLACEHOLDER_ACTION_ALTERNATIVE |
            FW_PLACEHOLDER_ACTION_SHOW_DETAILS;
    case FW_PLACEHOLDER_REASON_LOADING:
        return FW_PLACEHOLDER_ACTION_NONE;
    default:
        return FW_PLACEHOLDER_ACTION_RETRY |
            FW_PLACEHOLDER_ACTION_SHOW_DETAILS |
            FW_PLACEHOLDER_ACTION_ALTERNATIVE;
    }
}

static fw_placeholder_action_mask ph_first_action(
    fw_placeholder_action_mask actions) {
    return actions & (~actions + 1u);
}

static fw_placeholder_visual_density ph_visual_density(
    const fw_placeholder_request_v1 *request,
    fw_rect_f32 bounds) {
    const float em = request->constraints.em_size * request->target.font_scale;
    if (request->mode == FW_PLACEHOLDER_MODE_HIDDEN ||
        bounds.width == 0.0f || bounds.height == 0.0f) {
        return FW_PH_VISUAL_NONE;
    }
    if (request->mode == FW_PLACEHOLDER_MODE_MINIMAL ||
        bounds.width < 3.0f * em || bounds.height < 2.0f * em) {
        return FW_PH_VISUAL_OUTLINE;
    }
    if (bounds.width < 7.0f * em || bounds.height < 3.0f * em) {
        return FW_PH_VISUAL_ICON;
    }
    if (bounds.width < 10.0f * em || bounds.height < 4.0f * em) {
        return FW_PH_VISUAL_TITLE;
    }
    if (bounds.width < 12.0f * em || bounds.height < 6.0f * em) {
        return FW_PH_VISUAL_DETAIL;
    }
    return FW_PH_VISUAL_ACTIONS;
}

static fw_string_view ph_reason_symbol(fw_placeholder_reason reason) {
    switch (reason) {
    case FW_PLACEHOLDER_REASON_LOADING:
        return ph_static_view("placeholder.loading");
    case FW_PLACEHOLDER_REASON_POLICY_BLOCKED:
        return ph_static_view("placeholder.blocked");
    case FW_PLACEHOLDER_REASON_PERMISSION_REQUIRED:
        return ph_static_view("placeholder.permission");
    case FW_PLACEHOLDER_REASON_RESOURCE_MISSING:
        return ph_static_view("placeholder.missing");
    case FW_PLACEHOLDER_REASON_PARSE_FAILED:
    case FW_PLACEHOLDER_REASON_DECODE_FAILED:
    case FW_PLACEHOLDER_REASON_PLUGIN_FAILED:
        return ph_static_view("placeholder.error");
    default:
        return ph_static_view("placeholder.unavailable");
    }
}

static fw_string_view ph_reason_key(fw_placeholder_reason reason) {
    switch (reason) {
    case FW_PLACEHOLDER_REASON_LOADING:
        return ph_static_view("placeholder.status.loading");
    case FW_PLACEHOLDER_REASON_RENDERER_MISSING:
        return ph_static_view("placeholder.status.renderer_missing");
    case FW_PLACEHOLDER_REASON_UNSUPPORTED_TYPE:
        return ph_static_view("placeholder.status.unsupported_type");
    case FW_PLACEHOLDER_REASON_RESOURCE_MISSING:
        return ph_static_view("placeholder.status.resource_missing");
    case FW_PLACEHOLDER_REASON_RESOURCE_UNAVAILABLE:
        return ph_static_view("placeholder.status.resource_unavailable");
    case FW_PLACEHOLDER_REASON_PARSE_FAILED:
        return ph_static_view("placeholder.status.parse_failed");
    case FW_PLACEHOLDER_REASON_DECODE_FAILED:
        return ph_static_view("placeholder.status.decode_failed");
    case FW_PLACEHOLDER_REASON_POLICY_BLOCKED:
        return ph_static_view("placeholder.status.policy_blocked");
    case FW_PLACEHOLDER_REASON_PERMISSION_REQUIRED:
        return ph_static_view("placeholder.status.permission_required");
    case FW_PLACEHOLDER_REASON_RESOURCE_LIMITED:
        return ph_static_view("placeholder.status.resource_limited");
    case FW_PLACEHOLDER_REASON_PLUGIN_FAILED:
        return ph_static_view("placeholder.status.plugin_failed");
    default:
        return ph_static_view("placeholder.status.unknown");
    }
}

static fw_rect_f32 ph_action_rect(
    const fw_placeholder_request_v1 *request,
    fw_rect_f32 bounds) {
    fw_rect_f32 rect;
    const float em = request->constraints.em_size * request->target.font_scale;
    const float padding = request->style.content_padding;
    rect.width = 7.0f * em;
    if (rect.width > bounds.width - 2.0f * padding) {
        rect.width = bounds.width - 2.0f * padding;
    }
    rect.height = 2.25f * em;
    if (rect.height > bounds.height - 2.0f * padding) {
        rect.height = bounds.height - 2.0f * padding;
    }
    if (rect.width < 0.0f) {
        rect.width = 0.0f;
    }
    if (rect.height < 0.0f) {
        rect.height = 0.0f;
    }
    rect.x = bounds.x + padding;
    rect.y = bounds.y + bounds.height - padding - rect.height;
    return rect;
}

static uint64_t ph_hash_byte(uint64_t hash, uint8_t value) {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
    return hash;
}

static uint64_t ph_hash_bytes(uint64_t hash, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    for (index = 0u; index < length; ++index) {
        hash = ph_hash_byte(hash, bytes[index]);
    }
    return hash;
}

static uint64_t ph_hash_u32(uint64_t hash, uint32_t value) {
    uint32_t index;
    for (index = 0u; index < 4u; ++index) {
        hash = ph_hash_byte(hash, (uint8_t)(value & 0xffu));
        value >>= 8u;
    }
    return hash;
}

static uint64_t ph_hash_u64(uint64_t hash, uint64_t value) {
    uint32_t index;
    for (index = 0u; index < 8u; ++index) {
        hash = ph_hash_byte(hash, (uint8_t)(value & UINT64_C(0xff)));
        value >>= 8u;
    }
    return hash;
}

static uint64_t ph_hash_f32(uint64_t hash, float value) {
    uint32_t bits;
    if (value == 0.0f) {
        value = 0.0f;
    }
    memcpy(&bits, &value, sizeof(bits));
    return ph_hash_u32(hash, bits);
}

static uint64_t ph_hash_string(uint64_t hash, fw_string_view value) {
    hash = ph_hash_u64(hash, (uint64_t)value.length);
    return ph_hash_bytes(hash, value.data, value.length);
}

static uint64_t ph_hash_color(uint64_t hash, fw_color_rgba_f32 color) {
    hash = ph_hash_f32(hash, color.red);
    hash = ph_hash_f32(hash, color.green);
    hash = ph_hash_f32(hash, color.blue);
    return ph_hash_f32(hash, color.alpha);
}

static uint64_t ph_cache_hash(
    const fw_placeholder_request_v1 *request,
    fw_rect_f32 bounds,
    uint64_t seed) {
    uint64_t hash = seed;
    hash = ph_hash_u64(hash, request->request_id);
    hash = ph_hash_string(hash, request->zone_id);
    hash = ph_hash_string(hash, request->content_kind);
    hash = ph_hash_string(hash, request->required_capability_id);
    hash = ph_hash_string(hash, request->accessible_label);
    hash = ph_hash_string(hash, request->diagnostic_code);
    hash = ph_hash_u32(hash, request->reason);
    hash = ph_hash_u32(hash, request->mode);
    hash = ph_hash_u32(hash, request->permitted_actions);
    hash = ph_hash_f32(hash, request->constraints.min_width);
    hash = ph_hash_f32(hash, request->constraints.max_width);
    hash = ph_hash_f32(hash, request->constraints.min_height);
    hash = ph_hash_f32(hash, request->constraints.max_height);
    hash = ph_hash_f32(hash, request->constraints.em_size);
    hash = ph_hash_f32(hash, request->constraints.line_height);
    hash = ph_hash_u32(hash, request->constraints.flags);
    hash = ph_hash_color(hash, request->style.background);
    hash = ph_hash_color(hash, request->style.border);
    hash = ph_hash_color(hash, request->style.icon);
    hash = ph_hash_color(hash, request->style.primary_text);
    hash = ph_hash_color(hash, request->style.secondary_text);
    hash = ph_hash_color(hash, request->style.action);
    hash = ph_hash_f32(hash, request->style.opacity);
    hash = ph_hash_f32(hash, request->style.border_width);
    hash = ph_hash_f32(hash, request->style.corner_radius);
    hash = ph_hash_f32(hash, request->style.content_padding);
    hash = ph_hash_f32(hash, request->style.gap);
    hash = ph_hash_f32(hash, request->style.icon_size);
    hash = ph_hash_u32(hash, request->style.flags);
    hash = ph_hash_f32(hash, request->target.device_pixel_ratio);
    hash = ph_hash_f32(hash, request->target.font_scale);
    hash = ph_hash_u32(hash, request->target.medium);
    hash = ph_hash_u32(hash, request->target.prefers_dark);
    hash = ph_hash_u32(hash, request->target.high_contrast);
    hash = ph_hash_u32(hash, request->target.reduce_motion);
    hash = ph_hash_u32(hash, request->target.supports_alpha);
    hash = ph_hash_u32(hash, request->target.flags);
    hash = ph_hash_u32(hash, request->fragment_index);
    hash = ph_hash_u32(hash, request->fragment_count);
    hash = ph_hash_u64(hash, request->presentation_revision);
    hash = ph_hash_u32(hash, request->phase);
    hash = ph_hash_u32(hash, request->progress.kind);
    hash = ph_hash_u64(hash, request->progress.completed);
    hash = ph_hash_u64(hash, request->progress.total);
    hash = ph_hash_u32(hash, request->stale);
    hash = ph_hash_u32(hash, request->flags);
    hash = ph_hash_f32(hash, bounds.x);
    hash = ph_hash_f32(hash, bounds.y);
    hash = ph_hash_f32(hash, bounds.width);
    return ph_hash_f32(hash, bounds.height);
}

static fw_status ph_sink_failed(
    const fw_display_list_sink_v1 *sink,
    int saved) {
    if (saved && sink->restore != NULL) {
        (void)sink->restore(sink->user_data);
    }
    return FW_STATUS_SINK_REJECTED;
}

static fw_status FW_CALL ph_render(
    fw_plugin_handle plugin,
    const fw_placeholder_request_v1 *request,
    fw_rect_f32 bounds,
    const fw_placeholder_services_v1 *services,
    fw_placeholder_render_result_v1 *out_result) {
    fw_placeholder_normalization_flags flags;
    const char *diagnostic;
    fw_placeholder_request_v1 normalized;
    const fw_display_list_sink_v1 *sink;
    fw_placeholder_visual_density density;
    fw_placeholder_action_mask available;
    fw_placeholder_action_mask visible = FW_PLACEHOLDER_ACTION_NONE;
    fw_color_rgba_f32 color;
    fw_stroke_style_v1 stroke;
    fw_rect_f32 symbol_rect;
    fw_rect_f32 action_rect;
    fw_text_layout_handle layout = NULL;
    fw_text_layout_metrics_v1 metrics;
    fw_status status;
    uint32_t output_size;
    uint32_t commands = 0u;
    int saved = 0;
    (void)diagnostic;

    if (out_result == NULL ||
        out_result->struct_size < sizeof(fw_placeholder_render_result_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = ph_validate_core(plugin, request, &flags, &diagnostic);
    out_result->normalization_flags = flags;
    if (status != FW_STATUS_OK) {
        return status;
    }
    if (!isfinite(bounds.x) || !isfinite(bounds.y) ||
        !ph_is_finite_nonnegative(bounds.width) ||
        !ph_is_finite_nonnegative(bounds.height) || services == NULL ||
        services->struct_size < sizeof(fw_placeholder_services_v1) ||
        services->display_list == NULL ||
        services->display_list->struct_size < sizeof(fw_display_list_sink_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    sink = services->display_list;
    if (sink->save == NULL || sink->restore == NULL ||
        sink->clip_rect == NULL || sink->fill_rounded_rect == NULL ||
        sink->stroke_rounded_rect == NULL || sink->draw_symbol == NULL ||
        sink->draw_text_layout == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    ph_normalize_request(request, &normalized);
    density = ph_visual_density(&normalized, bounds);
    out_result->visual_density = density;
    if (density == FW_PH_VISUAL_NONE) {
        out_result->cache_key_high = ph_cache_hash(
            &normalized, bounds, UINT64_C(1469598103934665603));
        out_result->cache_key_low = ph_cache_hash(
            &normalized, bounds, UINT64_C(1099511628211));
        return FW_STATUS_OK;
    }
    available = normalized.permitted_actions &
        ph_supported_actions(normalized.reason);
    if (density >= FW_PH_VISUAL_ACTIONS) {
        visible = ph_first_action(available);
    }
    if (density >= FW_PH_VISUAL_TITLE && services->text != NULL &&
        services->text->struct_size >= sizeof(fw_text_service_v1) &&
        services->text->layout_utf8 != NULL &&
        services->text->release_layout != NULL &&
        normalized.accessible_label.length > 0u) {
        fw_text_layout_request_v1 text_request;
        memset(&text_request, 0, sizeof(text_request));
        memset(&metrics, 0, sizeof(metrics));
        text_request.struct_size = sizeof(text_request);
        text_request.text = normalized.accessible_label;
        text_request.locale = services->locale;
        text_request.font_size = normalized.constraints.em_size *
            normalized.target.font_scale;
        text_request.max_width = bounds.width -
            2.0f * normalized.style.content_padding;
        if (text_request.max_width < 0.0f) {
            text_request.max_width = 0.0f;
        }
        text_request.max_lines = density >= FW_PH_VISUAL_DETAIL ? 2u : 1u;
        text_request.direction = services->text_direction;
        text_request.ellipsize = 1u;
        metrics.struct_size = sizeof(metrics);
        status = services->text->layout_utf8(
            services->text->user_data, &text_request, &layout, &metrics);
        if (status != FW_STATUS_OK || layout == NULL) {
            layout = NULL;
            density = FW_PH_VISUAL_ICON;
            visible = FW_PLACEHOLDER_ACTION_NONE;
            out_result->visual_density = density;
            out_result->flags |= 1u;
        }
    }
    status = sink->save(sink->user_data);
    if (status != FW_STATUS_OK) {
        status = FW_STATUS_SINK_REJECTED;
        goto cleanup;
    }
    saved = 1;
    ++commands;
    status = sink->clip_rect(sink->user_data, bounds);
    if (status != FW_STATUS_OK) {
        status = ph_sink_failed(sink, saved);
        goto cleanup;
    }
    ++commands;
    color = ph_normalize_color(
        normalized.style.background, normalized.style.opacity);
    if (color.alpha > 0.0f) {
        status = sink->fill_rounded_rect(
            sink->user_data, bounds, normalized.style.corner_radius, color);
        if (status != FW_STATUS_OK) {
            status = ph_sink_failed(sink, saved);
            goto cleanup;
        }
        ++commands;
    }
    color = ph_normalize_color(
        normalized.style.border, normalized.style.opacity);
    if (density >= FW_PH_VISUAL_OUTLINE && color.alpha > 0.0f &&
        normalized.style.border_width > 0.0f) {
        stroke.struct_size = sizeof(stroke);
        stroke.color = color;
        stroke.width = normalized.style.border_width;
        stroke.dashed = normalized.reason == FW_PLACEHOLDER_REASON_LOADING;
        status = sink->stroke_rounded_rect(
            sink->user_data, bounds, normalized.style.corner_radius, &stroke);
        if (status != FW_STATUS_OK) {
            status = ph_sink_failed(sink, saved);
            goto cleanup;
        }
        ++commands;
    }
    color = ph_normalize_color(normalized.style.icon, normalized.style.opacity);
    if (density >= FW_PH_VISUAL_ICON && color.alpha > 0.0f) {
        const float icon_size = normalized.style.icon_size > 0.0f ?
            normalized.style.icon_size : normalized.constraints.em_size * 2.0f;
        symbol_rect.x = bounds.x + normalized.style.content_padding;
        symbol_rect.y = bounds.y + normalized.style.content_padding;
        symbol_rect.width = icon_size;
        symbol_rect.height = icon_size;
        status = sink->draw_symbol(
            sink->user_data, ph_reason_symbol(normalized.reason),
            symbol_rect, color);
        if (status != FW_STATUS_OK) {
            status = ph_sink_failed(sink, saved);
            goto cleanup;
        }
        ++commands;
    }
    if (layout != NULL && density >= FW_PH_VISUAL_TITLE) {
        fw_point_f32 origin;
        origin.x = bounds.x + normalized.style.content_padding;
        origin.y = bounds.y + normalized.style.content_padding;
        color = ph_normalize_color(
            normalized.style.primary_text, normalized.style.opacity);
        if (color.alpha > 0.0f) {
            status = sink->draw_text_layout(
                sink->user_data, layout, origin, color);
            if (status != FW_STATUS_OK) {
                status = ph_sink_failed(sink, saved);
                goto cleanup;
            }
            ++commands;
        }
    }
    if (visible != FW_PLACEHOLDER_ACTION_NONE) {
        action_rect = ph_action_rect(&normalized, bounds);
        color = ph_normalize_color(
            normalized.style.action, normalized.style.opacity);
        if (color.alpha <= 0.0f || action_rect.width <= 0.0f ||
            action_rect.height <= 0.0f) {
            visible = FW_PLACEHOLDER_ACTION_NONE;
        } else {
            status = sink->fill_rounded_rect(
                sink->user_data, action_rect,
                normalized.style.corner_radius, color);
            if (status != FW_STATUS_OK) {
                status = ph_sink_failed(sink, saved);
                goto cleanup;
            }
            ++commands;
        }
    }
    status = sink->restore(sink->user_data);
    if (status != FW_STATUS_OK) {
        status = FW_STATUS_SINK_REJECTED;
        saved = 0;
        goto cleanup;
    }
    saved = 0;
    ++commands;
    status = FW_STATUS_OK;

cleanup:
    if (layout != NULL) {
        services->text->release_layout(services->text->user_data, layout);
    }
    if (status == FW_STATUS_OK) {
        out_result->emitted_command_count = commands;
        out_result->visible_actions = visible;
        out_result->cache_key_high = ph_cache_hash(
            &normalized, bounds, UINT64_C(1469598103934665603));
        out_result->cache_key_low = ph_cache_hash(
            &normalized, bounds, UINT64_C(1099511628211));
    } else {
        out_result->emitted_command_count = 0u;
        out_result->visual_density = FW_PH_VISUAL_NONE;
        out_result->visible_actions = FW_PLACEHOLDER_ACTION_NONE;
        out_result->cache_key_high = 0u;
        out_result->cache_key_low = 0u;
        out_result->flags = 0u;
    }
    return status;
}

static fw_semantics_role ph_semantics_role(fw_string_view content_kind) {
    if (ph_string_equal(content_kind, "image")) {
        return FW_SEMANTICS_ROLE_IMAGE;
    }
    if (ph_string_equal(content_kind, "video") ||
        ph_string_equal(content_kind, "audio")) {
        return FW_SEMANTICS_ROLE_MEDIA;
    }
    if (ph_string_equal(content_kind, "document") ||
        ph_string_equal(content_kind, "text")) {
        return FW_SEMANTICS_ROLE_DOCUMENT;
    }
    if (ph_string_equal(content_kind, "chart")) {
        return FW_SEMANTICS_ROLE_CHART;
    }
    return FW_SEMANTICS_ROLE_CONTENT_UNAVAILABLE;
}

static fw_status FW_CALL ph_build_semantics(
    fw_plugin_handle plugin,
    const fw_placeholder_request_v1 *request,
    fw_rect_f32 bounds,
    fw_placeholder_semantics_v1 *out_semantics) {
    fw_placeholder_normalization_flags flags;
    const char *diagnostic;
    fw_placeholder_request_v1 normalized;
    fw_status status;
    uint32_t output_size;
    (void)flags;
    (void)diagnostic;
    if (out_semantics == NULL ||
        out_semantics->struct_size < sizeof(fw_placeholder_semantics_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_semantics->struct_size;
    memset(out_semantics, 0, sizeof(*out_semantics));
    out_semantics->struct_size = output_size;
    status = ph_validate_core(plugin, request, &flags, &diagnostic);
    if (status != FW_STATUS_OK || !isfinite(bounds.x) ||
        !isfinite(bounds.y) || !ph_is_finite_nonnegative(bounds.width) ||
        !ph_is_finite_nonnegative(bounds.height)) {
        return status == FW_STATUS_OK ? FW_STATUS_INVALID_ARGUMENT : status;
    }
    ph_normalize_request(request, &normalized);
    out_semantics->role = ph_semantics_role(normalized.content_kind);
    out_semantics->accessible_label = normalized.accessible_label;
    out_semantics->status_localization_key = ph_reason_key(normalized.reason);
    out_semantics->available_actions = normalized.permitted_actions &
        ph_supported_actions(normalized.reason);
    out_semantics->bounds = bounds;
    out_semantics->hidden_visually =
        normalized.mode == FW_PLACEHOLDER_MODE_HIDDEN;
    out_semantics->stale = normalized.stale;
    out_semantics->phase = normalized.phase;
    return FW_STATUS_OK;
}

static fw_status FW_CALL ph_hit_test(
    fw_plugin_handle plugin,
    const fw_placeholder_hit_test_request_v1 *request,
    fw_placeholder_hit_test_result_v1 *out_result) {
    fw_placeholder_normalization_flags flags;
    const char *diagnostic;
    fw_placeholder_request_v1 normalized;
    fw_placeholder_visual_density density;
    fw_placeholder_action_mask available;
    fw_placeholder_action_mask visible;
    fw_rect_f32 rect;
    fw_status status;
    uint32_t output_size;
    (void)flags;
    (void)diagnostic;
    if (out_result == NULL ||
        out_result->struct_size < sizeof(fw_placeholder_hit_test_result_v1) ||
        request == NULL ||
        request->struct_size < sizeof(fw_placeholder_hit_test_request_v1)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    output_size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = output_size;
    status = ph_validate_core(
        plugin, &request->placeholder, &flags, &diagnostic);
    if (status != FW_STATUS_OK || !isfinite(request->bounds.x) ||
        !isfinite(request->bounds.y) ||
        !ph_is_finite_nonnegative(request->bounds.width) ||
        !ph_is_finite_nonnegative(request->bounds.height) ||
        !isfinite(request->point.x) || !isfinite(request->point.y)) {
        return status == FW_STATUS_OK ? FW_STATUS_INVALID_ARGUMENT : status;
    }
    ph_normalize_request(&request->placeholder, &normalized);
    density = ph_visual_density(&normalized, request->bounds);
    available = normalized.permitted_actions &
        ph_supported_actions(normalized.reason);
    visible = density >= FW_PH_VISUAL_ACTIONS ?
        ph_first_action(available) : FW_PLACEHOLDER_ACTION_NONE;
    if (visible == FW_PLACEHOLDER_ACTION_NONE) {
        return FW_STATUS_OK;
    }
    rect = ph_action_rect(&normalized, request->bounds);
    if (ph_normalize_color(normalized.style.action,
            normalized.style.opacity).alpha <= 0.0f ||
        rect.width <= 0.0f || rect.height <= 0.0f) {
        return FW_STATUS_OK;
    }
    if (request->point.x >= rect.x && request->point.x < rect.x + rect.width &&
        request->point.y >= rect.y && request->point.y < rect.y + rect.height) {
        out_result->hit = 1u;
        out_result->action = visible;
    }
    return FW_STATUS_OK;
}

static fw_status FW_CALL ph_get_parameter_schema(
    fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!ph_context_is_valid(plugin) || out_schema_json == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    out_schema_json->data = ph_parameter_schema;
    out_schema_json->length = sizeof(ph_parameter_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_placeholder_renderer_api_v1 ph_renderer_api = {
    sizeof(fw_placeholder_renderer_api_v1),
    FW_PLACEHOLDER_RENDERER_INTERFACE_VERSION,
    ph_validate,
    ph_measure,
    ph_render,
    ph_build_semantics,
    ph_hit_test,
    ph_get_parameter_schema,
};

static const fw_plugin_descriptor_v1 *FW_CALL ph_get_descriptor(void) {
    return &ph_descriptor;
}

static fw_status FW_CALL ph_load(
    const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    ph_context *context;
    if (out_handle == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_handle = NULL;
    if (host == NULL || host->struct_size < sizeof(fw_host_api_v1) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    context = (ph_context *)calloc(1u, sizeof(*context));
    if (context == NULL) {
        return FW_STATUS_OUT_OF_MEMORY;
    }
    context->magic = PH_CONTEXT_MAGIC;
    context->host = *host;
    *out_handle = context;
    return FW_STATUS_OK;
}

static void FW_CALL ph_unload(fw_plugin_handle handle) {
    ph_context *context = (ph_context *)handle;
    if (context == NULL) {
        return;
    }
    if (context->magic == PH_CONTEXT_MAGIC) {
        context->magic = 0u;
        free(context);
    }
}

static fw_status FW_CALL ph_query_interface(
    fw_plugin_handle handle,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_interface = NULL;
    if (!ph_context_is_valid(handle) ||
        !ph_string_shape_is_valid(interface_id)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!ph_string_equal(interface_id, FW_PLACEHOLDER_RENDERER_INTERFACE_ID) ||
        minimum_version > FW_PLACEHOLDER_RENDERER_INTERFACE_VERSION) {
        return FW_STATUS_NOT_FOUND;
    }
    *out_interface = &ph_renderer_api;
    return FW_STATUS_OK;
}

static const fw_plugin_api_v1 ph_plugin_api = {
    sizeof(fw_plugin_api_v1),
    FW_ABI_VERSION_INIT,
    ph_get_descriptor,
    ph_load,
    ph_unload,
    ph_query_interface,
};

FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) {
        return NULL;
    }
    return &ph_plugin_api;
}
