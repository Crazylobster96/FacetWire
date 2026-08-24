/* SPDX-License-Identifier: MPL-2.0 */
#include "facetwire_placeholder_demo.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                    \
    do {                                                                     \
        if (!(expression)) {                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                \
                __FILE__, __LINE__, #expression);                            \
            return 1;                                                        \
        }                                                                    \
    } while (0)

static int contains(const fwdemo_buffer *buffer, const char *needle) {
    const size_t needle_length = strlen(needle);
    uint64_t index;
    if (buffer == NULL || needle == NULL || needle_length == 0u ||
        buffer->length < needle_length) {
        return 0;
    }
    for (index = 0u; index + needle_length <= buffer->length; ++index) {
        if (memcmp(buffer->data + index, needle, needle_length) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    static const char kind[] = "chart";
    static const char label[] = "Chart renderer is not installed";
    fwdemo_context *context = NULL;
    fwdemo_buffer output = {0};
    fwdemo_request_v1 request;
    uint32_t hit = 0u;
    uint32_t action = 0u;

    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.width = 640.0f;
    request.height = 360.0f;
    request.opacity = 0.5f;
    request.background_alpha = 0.8f;
    request.font_scale = 1.0f;
    request.device_pixel_ratio = 1.0f;
    request.reason = 2u;
    request.mode = 3u;
    request.permitted_actions = 63u;
    request.phase = 5u;
    request.progress_kind = 2u;
    request.completed = 3u;
    request.total = 4u;
    request.presentation_revision = 7u;
    request.content_kind_utf8 = kind;
    request.content_kind_length = sizeof(kind) - 1u;
    request.label_utf8 = label;
    request.label_length = sizeof(label) - 1u;

    CHECK(fwdemo_context_create(&context) == 0);
    CHECK(context != NULL);
    CHECK(fwdemo_runtime_snapshot(context, &output) == 0);
    CHECK(contains(&output, "org.facetwire.reference.placeholder-renderer"));
    CHECK(contains(&output, "facetwire.renderer.placeholder"));
    fwdemo_buffer_release(&output);

    CHECK(fwdemo_parameter_schema(context, &output) == 0);
    CHECK(contains(&output, "cornerRadius"));
    fwdemo_buffer_release(&output);

    CHECK(fwdemo_render(context, &request, &output) == 0);
    CHECK(contains(&output, "\"validationStatus\":0"));
    CHECK(contains(&output, "\"visualDensity\":5"));
    CHECK(contains(&output, "\"commandCount\":"));
    CHECK(contains(&output, "\"alpha\":0.400000006"));
    CHECK(contains(&output, "placeholder.status.renderer_missing"));
    CHECK(contains(&output, "Chart renderer is not installed"));
    fwdemo_buffer_release(&output);

    CHECK(fwdemo_hit_test(
        context, &request, 16.0f, 330.0f, &hit, &action) == 0);
    CHECK(hit == 1u);
    CHECK(action != 0u);

    fwdemo_context_destroy(context);
    puts("FacetWire placeholder demo bridge tests passed.");
    return 0;
}
