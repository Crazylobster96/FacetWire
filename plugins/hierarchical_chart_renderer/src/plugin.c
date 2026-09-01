/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/hierarchical_chart_renderer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HC_MAGIC UINT64_C(0x4641484945523031)
#define HC_PI 3.14159265358979323846
#define HC_MAX_NODES 2048u
#define HC_MAX_DEPTH 64u
#define HC_MAX_COMMANDS 65536u

typedef struct hc_context { uint64_t magic; fw_host_api_v1 host; } hc_context;
typedef struct hc_shape {
    uint32_t node;
    uint32_t depth;
    fw_rect_f32 rect;
    fw_point_f32 center;
    float radius;
    float inner_radius;
    float start;
    float sweep;
} hc_shape;
typedef struct hc_plan {
    hc_shape *shapes;
    size_t count;
    size_t capacity;
} hc_plan;

static const fw_capability_descriptor_v1 hc_capabilities[] = {{
    sizeof(fw_capability_descriptor_v1),
    FW_STRING_VIEW_LITERAL(FW_HIERARCHICAL_CHART_CAPABILITY_ID),
    FW_STRING_VIEW_LITERAL("facetwire.capability.renderer"), 15u}};
static const fw_plugin_descriptor_v1 hc_descriptor = {
    sizeof(fw_plugin_descriptor_v1), FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.hierarchical-chart-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Hierarchical Chart Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"), hc_capabilities, 1u};
static const char hc_schema[] =
    "{\"schemaVersion\":1,\"kinds\":[\"treemap\",\"sunburst\","
    "\"packed-bubble\"],\"opacity\":{\"minimum\":0,\"maximum\":1},"
    "\"uncoveredPixels\":\"transparent\",\"hierarchy\":\"parent-first\"}";

static int hc_context_valid(fw_plugin_handle plugin) {
    const hc_context *context = (const hc_context *)plugin;
    return context != NULL && context->magic == HC_MAGIC;
}
static int hc_string_shape(fw_string_view value) {
    return value.data != NULL || value.length == 0u;
}
static int hc_string_equal(fw_string_view value, const char *literal) {
    const size_t length = strlen(literal);
    return value.length == length &&
        (length == 0u || memcmp(value.data, literal, length) == 0);
}
static int hc_color_valid(fw_color_rgba_f32 value) {
    return isfinite(value.red) && isfinite(value.green) &&
        isfinite(value.blue) && isfinite(value.alpha) &&
        value.red >= 0.0f && value.red <= 1.0f &&
        value.green >= 0.0f && value.green <= 1.0f &&
        value.blue >= 0.0f && value.blue <= 1.0f &&
        value.alpha >= 0.0f && value.alpha <= 1.0f;
}
static int hc_rect_valid(fw_rect_f32 value) {
    return isfinite(value.x) && isfinite(value.y) &&
        isfinite(value.width) && isfinite(value.height) &&
        value.width >= 0.0f && value.height >= 0.0f;
}
static fw_size_f32 hc_intrinsic(
    const fw_hierarchical_chart_request_v1 *request) {
    fw_size_f32 result = request->intrinsic_size;
    if (!isfinite(result.width) || result.width <= 0.0f) result.width = 800.0f;
    if (!isfinite(result.height) || result.height <= 0.0f) result.height = 520.0f;
    return result;
}
static uint32_t hc_limit(uint32_t requested, uint32_t fallback) {
    return requested == 0u ? fallback : requested;
}

static fw_status hc_validate_request(fw_plugin_handle plugin,
    const fw_hierarchical_chart_request_v1 *request, const char **key) {
    size_t index;
    if (key != NULL) *key = "hierarchical.ok";
    if (!hc_context_valid(plugin) || request == NULL ||
        request->struct_size < sizeof(*request)) {
        if (key != NULL) *key = "hierarchical.request.invalid";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (!hc_string_shape(request->zone_id) ||
        !hc_string_shape(request->chart_id) ||
        !hc_string_shape(request->title) ||
        !hc_string_shape(request->summary) ||
        request->kind < FW_HIERARCHICAL_CHART_TREEMAP ||
        request->kind > FW_HIERARCHICAL_CHART_PACKED_BUBBLE ||
        !isfinite(request->opacity) || request->opacity < 0.0f ||
        request->opacity > 1.0f ||
        fw_visual_transform_validate(&request->transform) != FW_STATUS_OK ||
        request->style.struct_size < sizeof(request->style) ||
        request->budget.struct_size < sizeof(request->budget) ||
        request->constraints.struct_size < sizeof(request->constraints) ||
        request->target.struct_size < sizeof(request->target) ||
        !isfinite(request->style.gap) || request->style.gap < 0.0f ||
        request->style.gap > 0.2f ||
        !isfinite(request->style.inner_radius) ||
        request->style.inner_radius < 0.0f ||
        request->style.inner_radius >= 0.9f ||
        !isfinite(request->style.label_scale) ||
        request->style.label_scale < 0.25f ||
        request->style.label_scale > 4.0f ||
        request->style.show_labels > 1u || request->style.show_values > 1u ||
        request->node_count == 0u || request->nodes == NULL) {
        if (key != NULL) *key = "hierarchical.request.fields";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (request->node_count > hc_limit(request->budget.max_nodes,
        HC_MAX_NODES) || request->node_count > SIZE_MAX / sizeof(hc_shape)) {
        if (key != NULL) *key = "hierarchical.budget.nodes";
        return FW_STATUS_RESOURCE_LIMIT;
    }
    if (request->node_count >
        (hc_limit(request->budget.max_commands, HC_MAX_COMMANDS) - 4u) / 4u) {
        if (key != NULL) *key = "hierarchical.budget.commands";
        return FW_STATUS_RESOURCE_LIMIT;
    }
    for (index = 0u; index < request->node_count; ++index) {
        const fw_hierarchical_chart_node_v1 *node = &request->nodes[index];
        uint32_t depth = 0u;
        uint32_t parent = node->parent_index;
        if (node->struct_size < sizeof(*node) || !hc_string_shape(node->id) ||
            !hc_string_shape(node->label) || !isfinite(node->value) ||
            node->value < 0.0 || !hc_color_valid(node->color) ||
            node->visible > 1u ||
            (index == 0u && parent != FW_HIERARCHICAL_ROOT_INDEX) ||
            (index != 0u && parent >= index)) {
            if (key != NULL) *key = "hierarchical.node.invalid";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        if (index != 0u && node->visible != 0u &&
            request->nodes[parent].visible == 0u) {
            if (key != NULL) *key = "hierarchical.node.hidden-parent";
            return FW_STATUS_INVALID_ARGUMENT;
        }
        while (parent != FW_HIERARCHICAL_ROOT_INDEX) {
            ++depth;
            if (depth > hc_limit(request->budget.max_depth, HC_MAX_DEPTH)) {
                if (key != NULL) *key = "hierarchical.budget.depth";
                return FW_STATUS_RESOURCE_LIMIT;
            }
            parent = request->nodes[parent].parent_index;
        }
    }
    if (request->nodes[0].value <= 0.0) {
        if (key != NULL) *key = "hierarchical.root.value";
        return FW_STATUS_INVALID_ARGUMENT;
    }
    return FW_STATUS_OK;
}

static uint32_t hc_depth_of(const fw_hierarchical_chart_request_v1 *request,
    uint32_t node) {
    uint32_t depth = 0u;
    uint32_t parent = request->nodes[node].parent_index;
    while (parent != FW_HIERARCHICAL_ROOT_INDEX) {
        ++depth;
        parent = request->nodes[parent].parent_index;
    }
    return depth;
}
static int hc_add_shape(hc_plan *plan, hc_shape shape) {
    if (plan->count >= plan->capacity) return 0;
    plan->shapes[plan->count++] = shape;
    return 1;
}
static double hc_child_total(
    const fw_hierarchical_chart_request_v1 *request, uint32_t parent) {
    size_t index;
    double total = 0.0;
    for (index = (size_t)parent + 1u; index < request->node_count; ++index)
        if (request->nodes[index].parent_index == parent &&
            request->nodes[index].visible != 0u)
            total += request->nodes[index].value;
    return total;
}

static int hc_treemap(const fw_hierarchical_chart_request_v1 *request,
    uint32_t parent, fw_rect_f32 bounds, uint32_t depth, hc_plan *plan) {
    const double total = hc_child_total(request, parent);
    const float gap = request->style.gap;
    double cursor = depth % 2u == 0u ? bounds.x : bounds.y;
    size_t index;
    if (total <= 0.0) return 1;
    for (index = (size_t)parent + 1u; index < request->node_count; ++index) {
        const fw_hierarchical_chart_node_v1 *node = &request->nodes[index];
        hc_shape shape;
        fw_rect_f32 child;
        const float ratio = (float)(node->value / total);
        if (node->parent_index != parent || node->visible == 0u) continue;
        child = bounds;
        if (depth % 2u == 0u) {
            child.x = (float)cursor; child.width = bounds.width * ratio;
            cursor += child.width;
        } else {
            child.y = (float)cursor; child.height = bounds.height * ratio;
            cursor += child.height;
        }
        child.x += gap * 0.5f; child.y += gap * 0.5f;
        child.width = fmaxf(0.0f, child.width - gap);
        child.height = fmaxf(0.0f, child.height - gap);
        memset(&shape, 0, sizeof(shape));
        shape.node = (uint32_t)index; shape.depth = depth; shape.rect = child;
        if (!hc_add_shape(plan, shape) ||
            !hc_treemap(request, (uint32_t)index, child, depth + 1u, plan))
            return 0;
    }
    return 1;
}

static int hc_sunburst(const fw_hierarchical_chart_request_v1 *request,
    uint32_t parent, float start, float sweep, uint32_t depth,
    uint32_t max_depth, hc_plan *plan) {
    const double total = hc_child_total(request, parent);
    const float base = request->style.inner_radius * 0.38f;
    const float ring = (0.38f - base) / (float)max_depth;
    float cursor = start;
    size_t index;
    if (total <= 0.0) return 1;
    for (index = (size_t)parent + 1u; index < request->node_count; ++index) {
        const fw_hierarchical_chart_node_v1 *node = &request->nodes[index];
        hc_shape shape;
        float part;
        if (node->parent_index != parent || node->visible == 0u) continue;
        part = sweep * (float)(node->value / total);
        memset(&shape, 0, sizeof(shape));
        shape.node = (uint32_t)index; shape.depth = depth;
        shape.center = (fw_point_f32){0.5f, 0.5f};
        shape.inner_radius = base + ring * (float)(depth - 1u);
        shape.radius = base + ring * (float)depth;
        shape.start = cursor;
        shape.sweep = fmaxf(0.0f, part - request->style.gap * 0.05f);
        if (!hc_add_shape(plan, shape) ||
            (depth < max_depth && !hc_sunburst(request, (uint32_t)index,
                cursor, part, depth + 1u, max_depth, plan))) return 0;
        cursor += part;
    }
    return 1;
}

static int hc_collides(const hc_plan *plan, fw_point_f32 center,
    float radius) {
    size_t index;
    for (index = 0u; index < plan->count; ++index) {
        const hc_shape *shape = &plan->shapes[index];
        const float dx = center.x - shape->center.x;
        const float dy = center.y - shape->center.y;
        const float distance = radius + shape->radius + 0.006f;
        if (dx * dx + dy * dy < distance * distance) return 1;
    }
    return 0;
}
static int hc_packed(const fw_hierarchical_chart_request_v1 *request,
    hc_plan *plan) {
    size_t index;
    double maximum = 0.0;
    for (index = 1u; index < request->node_count; ++index)
        if (request->nodes[index].visible != 0u &&
            request->nodes[index].value > maximum)
            maximum = request->nodes[index].value;
    if (maximum <= 0.0) return 1;
    for (index = 1u; index < request->node_count; ++index) {
        const fw_hierarchical_chart_node_v1 *node = &request->nodes[index];
        hc_shape shape;
        uint32_t step;
        if (node->visible == 0u || node->value <= 0.0) continue;
        memset(&shape, 0, sizeof(shape));
        shape.node = (uint32_t)index;
        shape.depth = hc_depth_of(request, (uint32_t)index);
        shape.radius = 0.03f + 0.095f *
            sqrtf((float)(node->value / maximum));
        for (step = 0u; step < 4096u; ++step) {
            const float angle = (float)step * 2.39996323f;
            const float distance = 0.0045f * sqrtf((float)step);
            shape.center.x = 0.5f + cosf(angle) * distance;
            shape.center.y = 0.51f + sinf(angle) * distance;
            if (shape.center.x - shape.radius >= 0.03f &&
                shape.center.x + shape.radius <= 0.97f &&
                shape.center.y - shape.radius >= 0.09f &&
                shape.center.y + shape.radius <= 0.95f &&
                !hc_collides(plan, shape.center, shape.radius)) break;
        }
        if (step == 4096u) return 0;
        if (!hc_add_shape(plan, shape)) return 0;
    }
    return 1;
}

static fw_status hc_build_plan(
    const fw_hierarchical_chart_request_v1 *request, hc_plan *plan) {
    size_t index;
    uint32_t max_depth = 1u;
    int success;
    memset(plan, 0, sizeof(*plan));
    plan->capacity = request->node_count;
    plan->shapes = (hc_shape *)calloc(plan->capacity, sizeof(hc_shape));
    if (plan->shapes == NULL) return FW_STATUS_OUT_OF_MEMORY;
    for (index = 1u; index < request->node_count; ++index) {
        const uint32_t depth = hc_depth_of(request, (uint32_t)index);
        if (depth > max_depth) max_depth = depth;
    }
    if (request->kind == FW_HIERARCHICAL_CHART_TREEMAP) {
        const fw_rect_f32 bounds = {0.06f, 0.1f, 0.88f, 0.82f};
        success = hc_treemap(request, 0u, bounds, 1u, plan);
    } else if (request->kind == FW_HIERARCHICAL_CHART_SUNBURST) {
        success = hc_sunburst(request, 0u, -(float)HC_PI * 0.5f,
            (float)HC_PI * 2.0f, 1u, max_depth, plan);
    } else success = hc_packed(request, plan);
    if (!success) {
        free(plan->shapes); memset(plan, 0, sizeof(*plan));
        return FW_STATUS_RESOURCE_LIMIT;
    }
    return FW_STATUS_OK;
}
static void hc_free_plan(hc_plan *plan) {
    free(plan->shapes); memset(plan, 0, sizeof(*plan));
}

static fw_status FW_CALL hc_validate(fw_plugin_handle plugin,
    const fw_hierarchical_chart_request_v1 *request,
    fw_chart_validation_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = hc_validate_request(plugin, request, &key);
    out_result->status = status;
    out_result->diagnostic_key.data = key;
    out_result->diagnostic_key.length = strlen(key);
    return FW_STATUS_OK;
}

static fw_status FW_CALL hc_measure(fw_plugin_handle plugin,
    const fw_hierarchical_chart_request_v1 *request,
    fw_chart_measure_result_v1 *out_result) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result))
        return FW_STATUS_INVALID_ARGUMENT;
    status = hc_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    out_result->intrinsic_size = hc_intrinsic(request);
    out_result->size = out_result->intrinsic_size;
    return FW_STATUS_OK;
}

static fw_status hc_emit_label(const fw_hierarchical_chart_request_v1 *request,
    const fw_chart_draw_sink_v1 *sink, const hc_shape *shape,
    uint32_t *commands, uint32_t *labels) {
    const fw_hierarchical_chart_node_v1 *node = &request->nodes[shape->node];
    fw_point_f32 anchor;
    float area;
    char value_text[128];
    fw_string_view text = node->label;
    if ((request->style.show_labels == 0u &&
         request->style.show_values == 0u) ||
        (request->style.max_visible_labels != 0u &&
            *labels >= request->style.max_visible_labels)) return FW_STATUS_OK;
    if (request->style.show_values != 0u) {
        const int label_length = node->label.length < 72u ?
            (int)node->label.length : 72;
        const int written = request->style.show_labels != 0u &&
            node->label.length != 0u ?
            snprintf(value_text, sizeof(value_text), "%.*s · %.3g",
                label_length, node->label.data, node->value) :
            snprintf(value_text, sizeof(value_text), "%.3g", node->value);
        if (written > 0) {
            text.data = value_text;
            text.length = (size_t)written < sizeof(value_text) ?
                (size_t)written : sizeof(value_text) - 1u;
        }
    }
    if (text.length == 0u) return FW_STATUS_OK;
    if (request->kind == FW_HIERARCHICAL_CHART_TREEMAP) {
        anchor.x = shape->rect.x + shape->rect.width * 0.5f;
        anchor.y = shape->rect.y + shape->rect.height * 0.5f;
        area = shape->rect.width * shape->rect.height;
    } else if (request->kind == FW_HIERARCHICAL_CHART_SUNBURST) {
        const float angle = shape->start + shape->sweep * 0.5f;
        const float radius = (shape->radius + shape->inner_radius) * 0.5f;
        anchor.x = shape->center.x + cosf(angle) * radius;
        anchor.y = shape->center.y + sinf(angle) * radius;
        area = shape->sweep * (shape->radius - shape->inner_radius);
    } else {
        anchor = shape->center;
        area = shape->radius * shape->radius * (float)HC_PI;
    }
    if (area < 0.012f) return FW_STATUS_OK;
    ++*commands; ++*labels;
    return sink->draw_label(sink->user_data, text, anchor,
        0.028f * request->style.label_scale,
        (fw_color_rgba_f32){0.08f, 0.1f, 0.15f, 1.0f}, node->id);
}

static uint64_t hc_hash_bytes(uint64_t hash, const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    for (index = 0u; index < count; ++index) {
        hash ^= bytes[index]; hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static uint64_t hc_hash_request(
    const fw_hierarchical_chart_request_v1 *request, uint64_t hash) {
    size_t index;
    hash = hc_hash_bytes(hash, &request->kind, sizeof(request->kind));
    hash = hc_hash_bytes(hash, &request->presentation_revision,
        sizeof(request->presentation_revision));
    for (index = 0u; index < request->node_count; ++index) {
        const fw_hierarchical_chart_node_v1 *node = &request->nodes[index];
        hash = hc_hash_bytes(hash, &node->parent_index,
            sizeof(node->parent_index));
        hash = hc_hash_bytes(hash, &node->value, sizeof(node->value));
        hash = hc_hash_bytes(hash, node->id.data, node->id.length);
    }
    return hash;
}

static fw_status FW_CALL hc_render(fw_plugin_handle plugin,
    const fw_hierarchical_chart_request_v1 *request, fw_rect_f32 viewport,
    const fw_chart_services_v1 *services,
    fw_chart_render_result_v1 *out_result) {
    const fw_chart_draw_sink_v1 *sink;
    const char *key;
    fw_status status;
    fw_status first = FW_STATUS_OK;
    fw_visual_transform_result_v1 transform;
    hc_plan plan;
    size_t index;
    uint32_t commands = 0u;
    uint32_t labels = 0u;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        services == NULL || services->struct_size < sizeof(*services) ||
        services->draw == NULL || !hc_rect_valid(viewport))
        return FW_STATUS_INVALID_ARGUMENT;
    sink = services->draw;
    if (sink->struct_size < sizeof(*sink) || sink->begin_chart == NULL ||
        sink->end_chart == NULL || sink->fill_rect == NULL ||
        sink->fill_circle == NULL || sink->fill_sector == NULL ||
        sink->draw_label == NULL) return FW_STATUS_INVALID_ARGUMENT;
    status = hc_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    memset(&transform, 0, sizeof(transform));
    transform.struct_size = sizeof(transform);
    status = fw_visual_transform_resolve(hc_intrinsic(request), viewport,
        &request->transform, &transform);
    if (status != FW_STATUS_OK) return status;
    status = hc_build_plan(request, &plan);
    if (status != FW_STATUS_OK) return status;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    out_result->transform = transform;
    out_result->uncovered_is_transparent = 1u;
    if (request->opacity == 0.0f || transform.destination.width == 0.0f ||
        transform.destination.height == 0.0f) {
        hc_free_plan(&plan); return FW_STATUS_OK;
    }
    status = sink->begin_chart(sink->user_data, &transform, request->opacity);
    if (status != FW_STATUS_OK) {
        hc_free_plan(&plan); return FW_STATUS_SINK_REJECTED;
    }
    ++commands;
    for (index = 0u; index < plan.count && first == FW_STATUS_OK; ++index) {
        const hc_shape *shape = &plan.shapes[index];
        const fw_hierarchical_chart_node_v1 *node = &request->nodes[shape->node];
        fw_color_rgba_f32 color = node->color;
        color.alpha *= request->opacity;
        if (request->kind == FW_HIERARCHICAL_CHART_TREEMAP)
            first = sink->fill_rect(sink->user_data, shape->rect, color,
                node->id, (fw_string_view){NULL, 0u});
        else if (request->kind == FW_HIERARCHICAL_CHART_SUNBURST)
            first = sink->fill_sector(sink->user_data, shape->center,
                shape->radius, shape->inner_radius, shape->start, shape->sweep,
                color, node->id, (fw_string_view){NULL, 0u});
        else
            first = sink->fill_circle(sink->user_data, shape->center,
                shape->radius, color, node->id,
                (fw_string_view){NULL, 0u});
        ++commands;
        if (first == FW_STATUS_OK)
            first = hc_emit_label(request, sink, shape, &commands, &labels);
    }
    status = sink->end_chart(sink->user_data);
    ++commands;
    if (first == FW_STATUS_OK) first = status;
    out_result->emitted_command_count = commands;
    out_result->rendered_series_count = 1u;
    out_result->rendered_value_count = (uint32_t)plan.count;
    out_result->cache_key_high = hc_hash_request(request,
        UINT64_C(1469598103934665603));
    out_result->cache_key_low = hc_hash_request(request,
        UINT64_C(7809847782465536322));
    hc_free_plan(&plan);
    return first == FW_STATUS_OK ? FW_STATUS_OK : FW_STATUS_SINK_REJECTED;
}

static fw_status FW_CALL hc_build_semantics(fw_plugin_handle plugin,
    const fw_hierarchical_chart_request_v1 *request, fw_rect_f32 bounds,
    fw_chart_semantics_v1 *out_semantics) {
    const char *key;
    fw_status status;
    uint32_t size;
    if (out_semantics == NULL ||
        out_semantics->struct_size < sizeof(*out_semantics) ||
        !hc_rect_valid(bounds)) return FW_STATUS_INVALID_ARGUMENT;
    status = hc_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    size = out_semantics->struct_size;
    memset(out_semantics, 0, sizeof(*out_semantics));
    out_semantics->struct_size = size;
    out_semantics->role = FW_SEMANTICS_ROLE_CHART;
    out_semantics->label = request->title;
    out_semantics->summary = request->summary;
    out_semantics->bounds = bounds;
    out_semantics->series_count = 1u;
    out_semantics->value_count = (uint32_t)(request->node_count - 1u);
    return FW_STATUS_OK;
}

static int hc_shape_contains(const fw_hierarchical_chart_request_v1 *request,
    const hc_shape *shape, fw_point_f32 point) {
    if (request->kind == FW_HIERARCHICAL_CHART_TREEMAP)
        return point.x >= shape->rect.x &&
            point.x <= shape->rect.x + shape->rect.width &&
            point.y >= shape->rect.y &&
            point.y <= shape->rect.y + shape->rect.height;
    if (request->kind == FW_HIERARCHICAL_CHART_PACKED_BUBBLE) {
        const float dx = point.x - shape->center.x;
        const float dy = point.y - shape->center.y;
        return dx * dx + dy * dy <= shape->radius * shape->radius;
    }
    {
        const float dx = point.x - shape->center.x;
        const float dy = point.y - shape->center.y;
        const float distance = sqrtf(dx * dx + dy * dy);
        float angle = atan2f(dy, dx);
        while (angle < shape->start) angle += (float)HC_PI * 2.0f;
        return distance >= shape->inner_radius && distance <= shape->radius &&
            angle <= shape->start + shape->sweep;
    }
}

static int hc_point_to_normalized(fw_point_f32 point,
    const fw_visual_transform_result_v1 *transform, fw_point_f32 *out) {
    if (transform->destination.width <= 0.0f ||
        transform->destination.height <= 0.0f) return 0;
    out->x = (point.x - transform->destination.x) /
        transform->destination.width;
    out->y = (point.y - transform->destination.y) /
        transform->destination.height;
    return out->x >= 0.0f && out->x <= 1.0f &&
        out->y >= 0.0f && out->y <= 1.0f;
}

static fw_status FW_CALL hc_hit_test(fw_plugin_handle plugin,
    const fw_hierarchical_chart_request_v1 *request, fw_rect_f32 bounds,
    fw_point_f32 point, fw_hierarchical_chart_hit_result_v1 *out_result) {
    const char *key;
    fw_status status;
    fw_visual_transform_result_v1 transform;
    fw_point_f32 normalized;
    hc_plan plan;
    size_t index;
    uint32_t size;
    if (out_result == NULL || out_result->struct_size < sizeof(*out_result) ||
        !hc_rect_valid(bounds) || !isfinite(point.x) || !isfinite(point.y))
        return FW_STATUS_INVALID_ARGUMENT;
    size = out_result->struct_size;
    memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = size;
    status = hc_validate_request(plugin, request, &key);
    (void)key;
    if (status != FW_STATUS_OK) return status;
    memset(&transform, 0, sizeof(transform));
    transform.struct_size = sizeof(transform);
    status = fw_visual_transform_resolve(hc_intrinsic(request), bounds,
        &request->transform, &transform);
    if (status != FW_STATUS_OK) return status;
    if (!hc_point_to_normalized(point, &transform, &normalized))
        return FW_STATUS_OK;
    out_result->normalized_point = normalized;
    status = hc_build_plan(request, &plan);
    if (status != FW_STATUS_OK) return status;
    for (index = plan.count; index > 0u; --index) {
        const hc_shape *shape = &plan.shapes[index - 1u];
        if (hc_shape_contains(request, shape, normalized)) {
            const fw_hierarchical_chart_node_v1 *node =
                &request->nodes[shape->node];
            out_result->hit = 1u;
            out_result->node_index = shape->node;
            out_result->node_id = node->id;
            out_result->value = node->value;
            break;
        }
    }
    hc_free_plan(&plan);
    return FW_STATUS_OK;
}

static fw_status FW_CALL hc_get_parameter_schema(fw_plugin_handle plugin,
    fw_string_view *out_schema_json) {
    if (!hc_context_valid(plugin) || out_schema_json == NULL)
        return FW_STATUS_INVALID_ARGUMENT;
    out_schema_json->data = hc_schema;
    out_schema_json->length = sizeof(hc_schema) - 1u;
    return FW_STATUS_OK;
}

static const fw_hierarchical_chart_api_v1 hc_renderer_api = {
    sizeof(fw_hierarchical_chart_api_v1),
    FW_HIERARCHICAL_CHART_INTERFACE_VERSION, hc_validate, hc_measure,
    hc_render, hc_build_semantics, hc_hit_test, hc_get_parameter_schema};

static const fw_plugin_descriptor_v1 *FW_CALL hc_get_descriptor(void) {
    return &hc_descriptor;
}
static fw_status FW_CALL hc_load(const fw_host_api_v1 *host,
    fw_plugin_handle *out_handle) {
    hc_context *context;
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL || host->struct_size < sizeof(*host) ||
        host->abi_version.major != FW_ABI_VERSION_MAJOR ||
        host->abi_version.minor > FW_ABI_VERSION_MINOR)
        return FW_STATUS_INVALID_ARGUMENT;
    context = (hc_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return FW_STATUS_OUT_OF_MEMORY;
    context->magic = HC_MAGIC;
    context->host = *host;
    *out_handle = context;
    return FW_STATUS_OK;
}
static void FW_CALL hc_unload(fw_plugin_handle handle) {
    hc_context *context = (hc_context *)handle;
    if (context != NULL && context->magic == HC_MAGIC) {
        context->magic = 0u; free(context);
    }
}
static fw_status FW_CALL hc_query_interface(fw_plugin_handle handle,
    fw_string_view id, uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (!hc_context_valid(handle) || !hc_string_shape(id))
        return FW_STATUS_INVALID_ARGUMENT;
    if (!hc_string_equal(id, FW_HIERARCHICAL_CHART_INTERFACE_ID) ||
        minimum_version > FW_HIERARCHICAL_CHART_INTERFACE_VERSION)
        return FW_STATUS_NOT_FOUND;
    *out_interface = &hc_renderer_api;
    return FW_STATUS_OK;
}
static const fw_plugin_api_v1 hc_plugin_api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    hc_get_descriptor, hc_load, hc_unload, hc_query_interface};

#if defined(FACETWIRE_HIERARCHICAL_CHART_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT
#endif
const fw_plugin_api_v1 *FW_CALL
facetwire_hierarchical_chart_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) return NULL;
    return &hc_plugin_api;
}

#if defined(FACETWIRE_HIERARCHICAL_CHART_DYNAMIC_ENTRY)
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    return facetwire_hierarchical_chart_plugin_query(requested_abi);
}
#endif
