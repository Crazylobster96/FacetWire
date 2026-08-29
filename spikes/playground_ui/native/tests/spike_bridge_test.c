/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_ui_spike.h"

/*
 * This executable is a contract test, including in Release configurations.
 * CMake defines NDEBUG for Release, which would otherwise compile out every
 * assertion and turn the test into a false positive.
 */
#if defined(NDEBUG)
#  undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t read_u32_le(const uint8_t *src) {
    return (uint32_t)src[0] |
        ((uint32_t)src[1] << 8u) |
        ((uint32_t)src[2] << 16u) |
        ((uint32_t)src[3] << 24u);
}

static float read_f32_le(const uint8_t *src) {
    const uint32_t bits = read_u32_le(src);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int main(void) {
    fwui_context *context = NULL;
    fwui_buffer display = {0};
    fwui_buffer semantics = {0};
    fwui_buffer snapshot = {0};
    fwui_buffer flow = {0};
    fwui_buffer second_display = {0};
    fwui_buffer second_semantics = {0};
    unsigned int iteration = 0u;
    unsigned int content_case = 0u;
    unsigned int page_mode = 0u;
    static const char *const expected_intro[3] = {
        "paragraph.intro.level-1",
        "paragraph.intro.level-2",
        "paragraph.intro.level-3"
    };
    static const char *const expected_page_count[3] = {
        "\"pageCount\":3", "\"pageCount\":3", "\"pageCount\":2"
    };
    static const char *const expected_last_page[3] = {
        "\"pageIndex\":2", "\"pageIndex\":2", "\"pageIndex\":1"
    };

    assert(fwui_context_create(NULL) == FWUI_STATUS_INVALID_ARGUMENT);
    assert(fwui_context_create(&context) == FWUI_STATUS_OK);
    assert(context != NULL);
    assert(fwui_runtime_snapshot(context, &snapshot) == FWUI_STATUS_OK);
    assert(snapshot.length > 0u);
    assert(strstr((const char *)snapshot.data, "placeholder") != NULL);
    assert(fwui_runtime_snapshot(context, &snapshot) ==
        FWUI_STATUS_INVALID_ARGUMENT);
    assert(snapshot.data != NULL && snapshot.length != 0u);
    fwui_buffer_release(&snapshot);

    assert(fwui_render_placeholder(context, NAN, 100.0f, 1.0f,
        &display, &semantics) == FWUI_STATUS_INVALID_ARGUMENT);
    assert(display.data == NULL && display.length == 0u);
    assert(fwui_render_placeholder(context, 200.0f, 100.0f, -0.1f,
        &display, &semantics) == FWUI_STATUS_INVALID_ARGUMENT);
    assert(fwui_render_placeholder(context, 200.0f, 100.0f, 0.5f,
        &display, &display) == FWUI_STATUS_INVALID_ARGUMENT);
    assert(display.data == NULL && display.length == 0u);

    assert(fwui_render_placeholder(context, 200.0f, 100.0f, 0.0f,
        &display, &semantics) == FWUI_STATUS_OK);
    assert(display.length == 132u);
    assert(memcmp(display.data, "FWDL", 4u) == 0);
    assert(display.data[4] == 1u && display.data[5] == 0u);
    assert(read_u32_le(display.data + 8u) == 3u);
    assert(fabsf(read_f32_le(display.data + 12u + 36u)) < 0.0001f);
    assert(strstr((const char *)semantics.data, "\"role\":\"image\"") != NULL);

    assert(fwui_render_placeholder(context, 200.0f, 100.0f, 0.0f,
        &display, &second_semantics) == FWUI_STATUS_INVALID_ARGUMENT);
    assert(display.data != NULL && display.length == 132u);
    assert(second_semantics.data == NULL && second_semantics.length == 0u);

    assert(fwui_render_placeholder(context, 200.0f, 100.0f, 0.0f,
        &second_display, &second_semantics) == FWUI_STATUS_OK);
    assert(second_display.length == display.length);
    assert(memcmp(second_display.data, display.data, (size_t)display.length) == 0);
    assert(second_semantics.length == semantics.length);
    assert(memcmp(second_semantics.data, semantics.data,
        (size_t)semantics.length) == 0);
    fwui_buffer_release(&display);
    fwui_buffer_release(&semantics);
    fwui_buffer_release(&second_display);
    fwui_buffer_release(&second_semantics);

    assert(fwui_render_placeholder(context, 200.0f, 100.0f, 1.0f,
        &display, &semantics) == FWUI_STATUS_OK);
    assert(fabsf(read_f32_le(display.data + 12u + 36u) - 1.0f) < 0.0001f);
    fwui_buffer_release(&display);
    fwui_buffer_release(&display);
    fwui_buffer_release(&semantics);

    assert(fwui_render_placeholder(context, 1.0f, 1.0f, 0.5f,
        &display, &semantics) == FWUI_STATUS_OK);
    assert(read_f32_le(display.data + 52u + 12u) >= 0.0f);
    assert(read_f32_le(display.data + 52u + 16u) >= 0.0f);
    assert(read_f32_le(display.data + 52u + 20u) >= 0.0f);
    fwui_buffer_release(&display);
    fwui_buffer_release(&semantics);

    assert(fwui_compose_flow_demo(context, 600.0f, 700.0f, 0u, &flow) ==
        FWUI_STATUS_OK);
    assert(strstr((const char *)flow.data,
        "\"capability\":\"facetwire.layout.flow\"") != NULL);
    assert(strstr((const char *)flow.data, "\"composeStatus\":0") != NULL);
    assert(strstr((const char *)flow.data, "\"fragmentCount\":3") != NULL);
    assert(strstr((const char *)flow.data, "paragraph.intro.level-1") != NULL);
    assert(strstr((const char *)flow.data, "\"pagesBalanced\":true") != NULL);
    fwui_buffer_release(&flow);

    assert(fwui_compose_flow_demo(context, 600.0f, 700.0f, 2u, &flow) ==
        FWUI_STATUS_OK);
    assert(strstr((const char *)flow.data, "\"kind\":\"placeholder\"") != NULL);
    assert(strstr((const char *)flow.data, "object.missing.level-3") != NULL);
    fwui_buffer_release(&flow);

    assert(fwui_compose_flow_demo(context, 600.0f, 700.0f, 3u, &flow) ==
        FWUI_STATUS_OK);
    assert(strstr((const char *)flow.data, "\"composeStatus\":0") != NULL);
    assert(strstr((const char *)flow.data, "\"complete\":true") != NULL);
    assert(strstr((const char *)flow.data, "\"pageCount\":3") != NULL);
    assert(strstr((const char *)flow.data, "\"fragmentCount\":3") != NULL);
    fwui_buffer_release(&flow);

    for (content_case = 0u; content_case < 3u; ++content_case) {
        for (page_mode = 0u; page_mode < 2u; ++page_mode) {
            assert(fwui_compose_flow_demo_v2(context, 600.0f, 700.0f,
                content_case, page_mode, &flow) == FWUI_STATUS_OK);
            assert(strstr((const char *)flow.data,
                expected_intro[content_case]) != NULL);
            if (page_mode == FWUI_FLOW_PAGE_VIRTUAL) {
                assert(strstr((const char *)flow.data,
                    "\"pageMode\":1") != NULL);
                assert(strstr((const char *)flow.data,
                    expected_page_count[content_case]) != NULL);
                assert(strstr((const char *)flow.data,
                    expected_last_page[content_case]) != NULL);
            } else {
                assert(strstr((const char *)flow.data,
                    "\"pageMode\":0") != NULL);
                assert(strstr((const char *)flow.data,
                    "\"pageCount\":1") != NULL);
            }
            fwui_buffer_release(&flow);
        }
    }
    assert(fwui_compose_flow_demo_v2(context, 600.0f, 700.0f, 3u,
        FWUI_FLOW_PAGE_CONTINUOUS, &flow) == FWUI_STATUS_INVALID_ARGUMENT);
    assert(fwui_compose_flow_demo_v2(context, 600.0f, 700.0f, 0u, 2u,
        &flow) == FWUI_STATUS_INVALID_ARGUMENT);

    assert(fwui_compose_flow_demo(context, 100.0f, 700.0f, 0u, &flow) ==
        FWUI_STATUS_INVALID_ARGUMENT);
    assert(flow.data == NULL && flow.length == 0u);

    for (iteration = 0u; iteration < 1000u; ++iteration) {
        assert(fwui_render_placeholder(context, 640.0f, 360.0f, 0.75f,
            &display, &semantics) == FWUI_STATUS_OK);
        fwui_buffer_release(&display);
        fwui_buffer_release(&semantics);
    }

    fwui_context_destroy(context);
    puts("FacetWire UI spike bridge tests passed.");
    return 0;
}
