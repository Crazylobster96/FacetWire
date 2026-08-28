/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/image_renderer.h>
#include <facetwire/flow_layout.h>
#include <facetwire/media_renderer.h>
#include <facetwire/placeholder_renderer.h>
#include <facetwire/runtime.h>
#include <facetwire/text_renderer.h>

#if defined(NDEBUG)
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

int main(void) {
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    const fw_runtime_config_v1 config = {
        sizeof(fw_runtime_config_v1), &host, 5u};
    fw_runtime *runtime = NULL;
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemState before;
    _CrtMemState after;
    _CrtMemState difference;
    _CrtMemCheckpoint(&before);
#endif

    assert(fw_runtime_create(&config, &runtime) == FW_STATUS_OK);
    assert(fw_runtime_register_static(runtime,
        facetwire_placeholder_renderer_plugin_query, NULL) == FW_STATUS_OK);
    assert(fw_runtime_register_static(runtime,
        facetwire_text_renderer_plugin_query, NULL) == FW_STATUS_OK);
    assert(fw_runtime_register_static(runtime,
        facetwire_core_image_plugin_query, NULL) == FW_STATUS_OK);
    assert(fw_runtime_register_static(runtime,
        facetwire_core_media_plugin_query, NULL) == FW_STATUS_OK);
    assert(fw_runtime_register_static(runtime,
        facetwire_flow_layout_plugin_query, NULL) == FW_STATUS_OK);
    assert(fw_runtime_plugin_count(runtime) == 5u);
    fw_runtime_destroy(runtime);

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemCheckpoint(&after);
    if (_CrtMemDifference(&difference, &before, &after) != 0) {
        _CrtMemDumpStatistics(&difference);
        return 1;
    }
#endif
    puts("FacetWire runtime and renderer lifetime check passed.");
    return 0;
}
