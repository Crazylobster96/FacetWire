/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/runtime.h>

#include <assert.h>
#include <string.h>

static int load_count;
static int unload_count;

static const fw_capability_descriptor_v1 capabilities[] = {
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.render"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.renderer"),
        0u,
    },
};

static const fw_plugin_descriptor_v1 descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.test.plugin"),
    FW_STRING_VIEW_LITERAL("Contract test plugin"),
    FW_STRING_VIEW_LITERAL("FacetWire"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    capabilities,
    1u,
};

static const fw_plugin_descriptor_v1 *FW_CALL get_descriptor(void) {
    return &descriptor;
}

static fw_status FW_CALL load_plugin(const fw_host_api_v1 *host,
                                     fw_plugin_handle *out_handle) {
    assert(host != NULL);
    assert(out_handle != NULL);
    ++load_count;
    *out_handle = (fw_plugin_handle)&descriptor;
    return FW_STATUS_OK;
}

static void FW_CALL unload_plugin(fw_plugin_handle handle) {
    assert(handle == (fw_plugin_handle)&descriptor);
    ++unload_count;
}

static const fw_plugin_api_v1 api = {
    sizeof(fw_plugin_api_v1),
    FW_ABI_VERSION_INIT,
    get_descriptor,
    load_plugin,
    unload_plugin,
    NULL,
};

static const fw_plugin_api_v1 *FW_CALL query_plugin(
    fw_abi_version requested_abi) {
    return requested_abi.major == FW_ABI_VERSION_MAJOR ? &api : NULL;
}

int main(void) {
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    const fw_runtime_config_v1 config = {
        sizeof(fw_runtime_config_v1), &host, 2u};
    const fw_plugin_descriptor_v1 *registered;
    fw_runtime *runtime = NULL;

    assert(strcmp(fw_status_name(FW_STATUS_OK), "ok") == 0);
    assert(fw_runtime_create(&config, &runtime) == FW_STATUS_OK);
    assert(runtime != NULL);
    assert(fw_runtime_plugin_count(runtime) == 0u);
    assert(fw_runtime_register_static(runtime, query_plugin, &registered) ==
           FW_STATUS_OK);
    assert(registered == &descriptor);
    assert(load_count == 1);
    assert(fw_runtime_plugin_count(runtime) == 1u);
    assert(fw_runtime_plugin_at(runtime, 0u) == &descriptor);
    assert(fw_runtime_plugin_at(runtime, 1u) == NULL);
    assert(fw_runtime_register_static(runtime, query_plugin, NULL) ==
           FW_STATUS_ALREADY_REGISTERED);

    fw_runtime_destroy(runtime);
    assert(unload_count == 1);
    return 0;
}
