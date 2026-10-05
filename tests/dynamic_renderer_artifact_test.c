/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/runtime.h>

#if defined(NDEBUG)
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static fw_string_view view(const char *text) {
    const fw_string_view result = {text, strlen(text)};
    return result;
}

static int equal(fw_string_view actual, const char *expected) {
    return actual.length == strlen(expected) && actual.data != NULL &&
           memcmp(actual.data, expected, actual.length) == 0;
}

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
    int needed;
    char *path;
    char *identity;
    char *capability;
    char *interface_id;
    char *converted[4];
    int index;
#else
int main(int argc, char **argv) {
#endif
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    const fw_runtime_config_v1 config = {
        sizeof(fw_runtime_config_v1), &host, 1u};
    const fw_plugin_descriptor_v1 *descriptor = NULL;
    const void *interface_value = NULL;
    fw_capability_match_v1 match = {
        sizeof(fw_capability_match_v1), 0u, NULL, NULL,
        FW_PLUGIN_SOURCE_UNKNOWN};
    fw_runtime *runtime = NULL;
    assert(argc == 5);
#if defined(_WIN32)
    for (index = 1; index < argc; ++index) {
        needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                     argv[index], -1, NULL, 0, NULL, NULL);
        assert(needed > 0);
        converted[index - 1] = (char *)malloc((size_t)needed);
        assert(converted[index - 1] != NULL);
        assert(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
               argv[index], -1, converted[index - 1], needed,
               NULL, NULL) == needed);
    }
    path = converted[0];
    identity = converted[1];
    capability = converted[2];
    interface_id = converted[3];
#else
    const char *path = argv[1];
    const char *identity = argv[2];
    const char *capability = argv[3];
    const char *interface_id = argv[4];
#endif
    assert(fw_runtime_create(&config, &runtime) == FW_STATUS_OK);
    assert(fw_runtime_load_dynamic(runtime, view(path), &descriptor) ==
           FW_STATUS_OK);
    assert(descriptor != NULL && equal(descriptor->id, identity));
    assert(fw_runtime_plugin_source_at(runtime, 0u) ==
           FW_PLUGIN_SOURCE_NATIVE_DYNAMIC);
    assert(fw_runtime_find_capability(runtime, view(capability), 0u,
                                      &match) == FW_STATUS_OK);
    assert(match.plugin_index == 0u && match.plugin == descriptor &&
           match.source == FW_PLUGIN_SOURCE_NATIVE_DYNAMIC);
    assert(fw_runtime_query_interface(runtime, view(identity),
                                      view(interface_id), 1u,
                                      &interface_value) == FW_STATUS_OK);
    assert(interface_value != NULL);
    assert(fw_runtime_load_dynamic(runtime, view(path), NULL) ==
           FW_STATUS_CAPACITY_EXCEEDED);
    assert(fw_runtime_unload_dynamic(runtime, view(identity)) == FW_STATUS_OK);
    assert(fw_runtime_plugin_count(runtime) == 0u);
    assert(fw_runtime_find_plugin(runtime, view(identity), NULL, NULL) ==
           FW_STATUS_NOT_FOUND);
    fw_runtime_destroy(runtime);
#if defined(_WIN32)
    for (index = 0; index < 4; ++index) free(converted[index]);
#endif
    return 0;
}
