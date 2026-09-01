/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_chart_demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #value); \
    return 1; } } while (0)

static int contains(const fwchart_buffer *buffer, const char *text) {
    const size_t length = strlen(text);
    size_t i;
    if (length > buffer->length) return 0;
    for (i = 0u; i + length <= buffer->length; ++i)
        if (memcmp(buffer->data + i, text, length) == 0) return 1;
    return 0;
}

static int element_index_for_id(const fwchart_buffer *buffer,
    const char *element_id, uint32_t *out_index) {
    const char *json;
    const char *match;
    const char *cursor;
    const char *candidate = NULL;
    if (buffer == NULL || buffer->data == NULL || element_id == NULL ||
        out_index == NULL) return 0;
    json = (const char *)buffer->data;
    match = strstr(json, element_id);
    if (match == NULL) return 0;
    cursor = json;
    while ((cursor = strstr(cursor, "\"index\":")) != NULL &&
        cursor < match) {
        candidate = cursor;
        cursor += 8;
    }
    if (candidate == NULL) return 0;
    *out_index = (uint32_t)strtoul(candidate + 8, NULL, 10);
    return 1;
}

int main(void) {
    fwchart_context *context = NULL;
    uint32_t kind;
    uint32_t revenue_q2_index = UINT32_MAX;
    char selected_index_json[64];
    CHECK(fwchart_context_create(&context) == FWCHART_STATUS_OK);
    for (kind = 0u; kind < 30u; ++kind) {
        fwchart_buffer output = {0};
        CHECK(fwchart_render_demo(context, 640.0f, 360.0f, kind, kind % 4u,
            0.72f, &output) == FWCHART_STATUS_OK);
        CHECK(output.data != NULL && output.length != 0u);
        CHECK(contains(&output, kind >= 27u ?
            "org.facetwire.reference.hierarchical-chart-renderer" :
            "org.facetwire.reference.core-chart-renderer"));
        CHECK(contains(&output, "\"nativeRuntime\":true"));
        CHECK(contains(&output, "\"commandsBalanced\":true"));
        CHECK(contains(&output, "\"uncoveredIsTransparent\":true"));
        CHECK(contains(&output, "\"commandCount\":"));
        CHECK(contains(&output, "\"elements\":["));
        if (kind < 27u)
            CHECK(contains(&output, "\"elementId\":\"chart/"));
        if (kind == 6u || kind == 7u || kind == 11u || kind == 17u)
            CHECK(contains(&output, "\"type\":\"polygon\""));
        if (kind == 2u || kind == 10u || kind == 13u)
            CHECK(contains(&output, "\"type\":\"sector\""));
        if (kind == 8u || kind == 9u || kind == 19u)
            CHECK(contains(&output, "\"type\":\"circle\""));
        if (kind == 23u)
            CHECK(contains(&output, "\"type\":\"polygon\""));
        if (kind == 26u || kind == 28u)
            CHECK(contains(&output, "\"type\":\"sector\""));
        if (kind == 27u)
            CHECK(contains(&output, "\"type\":\"rect\""));
        if (kind == 29u)
            CHECK(contains(&output, "\"type\":\"circle\""));
        fwchart_buffer_release(&output);
    }
    {
        fwchart_buffer output = {0};
        CHECK(fwchart_render_demo(context, 640.0f, 360.0f, 0u, 0u,
            0.9f, &output) == FWCHART_STATUS_OK);
        CHECK(element_index_for_id(&output,
            "\"id\":\"chart/chart%3Aquarterly/datum/revenue/q2\"",
            &revenue_q2_index));
        CHECK(snprintf(selected_index_json, sizeof(selected_index_json),
            "\"selectedElementIndex\":%u", revenue_q2_index) > 0);
        fwchart_buffer_release(&output);
    }
    {
        fwchart_buffer output = {0};
        CHECK(fwchart_render_presentation_demo(context, 640.0f, 360.0f,
            21u, 0u, 0.86f, 2u, 2u, 2u, 1u,
            &output) == FWCHART_STATUS_OK);
        CHECK(contains(&output, "\"kind\":\"diverging-bar\""));
        CHECK(contains(&output, "\"commandsBalanced\":true"));
        fwchart_buffer_release(&output);
    }
    {
        fwchart_buffer output = {0};
        CHECK(fwchart_render_presentation_elements_demo(context, 640.0f,
            360.0f, 0u, 0u, 0.9f, revenue_q2_index, 0.35f, 0.05f, -0.03f,
            1.12f, 0.25f, 1u, 1u, 2u, 2u, 2u, 1u,
            &output) == FWCHART_STATUS_OK);
        CHECK(contains(&output, selected_index_json));
        CHECK(contains(&output, "\"commandsBalanced\":true"));
        fwchart_buffer_release(&output);
    }
    {
        fwchart_buffer output = {0};
        CHECK(fwchart_render_elements_demo(context, 640.0f, 360.0f,
            0u, 0u, 0.9f, revenue_q2_index, 0.35f, 0.05f, -0.03f, 1.12f,
            0.25f, 1u, 1u, &output) == FWCHART_STATUS_OK);
        CHECK(contains(&output, selected_index_json));
        CHECK(contains(&output,
            "chart/chart%3Aquarterly/datum/revenue/q2"));
        CHECK(contains(&output, "\"promoted\":true"));
        CHECK(contains(&output, "\"zIndex\":150"));
        CHECK(contains(&output,
            "\"color\":[0.9400,0.4500,0.1600,0.3500]"));
        fwchart_buffer_release(&output);
    }
    CHECK(fwchart_render_demo(context, 0.0f, 360.0f, 0u, 0u, 1.0f,
        &(fwchart_buffer){0}) == FWCHART_STATUS_INVALID_ARGUMENT);
    fwchart_context_destroy(context);
    puts("core chart demo bridge passed");
    return 0;
}
