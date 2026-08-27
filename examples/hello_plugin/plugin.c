/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/facetwire.h>

static const fw_host_api_v1 *hello_host;

static const fw_capability_descriptor_v1 hello_capabilities[] = {
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("org.facetwire.example.hello.message"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.diagnostic"),
        0u,
    },
};

static const fw_plugin_descriptor_v1 hello_descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.example.hello"),
    FW_STRING_VIEW_LITERAL("FacetWire Hello Plugin"),
    FW_STRING_VIEW_LITERAL("FacetWire contributors"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    hello_capabilities,
    sizeof(hello_capabilities) / sizeof(hello_capabilities[0]),
};

static const fw_plugin_descriptor_v1 *FW_CALL hello_get_descriptor(void) {
    return &hello_descriptor;
}

static fw_status FW_CALL hello_load(const fw_host_api_v1 *host,
                                    fw_plugin_handle *out_handle) {
    static const fw_string_view target =
        FW_STRING_VIEW_LITERAL("org.facetwire.example.hello");
    static const fw_string_view message =
        FW_STRING_VIEW_LITERAL("hello plugin loaded");

    if (out_handle == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_handle = NULL;
    if (host == NULL) return FW_STATUS_INVALID_ARGUMENT;
    hello_host = host;
    *out_handle = (fw_plugin_handle)&hello_descriptor;
    if (hello_host->log != NULL) {
        hello_host->log(hello_host->user_data, FW_LOG_INFO, target, message);
    }
    return FW_STATUS_OK;
}

static void FW_CALL hello_unload(fw_plugin_handle handle) {
    (void)handle;
    hello_host = NULL;
}

static fw_status FW_CALL hello_query_interface(fw_plugin_handle handle,
                                               fw_string_view interface_id,
                                               uint32_t minimum_version,
                                               const void **out_interface) {
    (void)handle;
    (void)interface_id;
    (void)minimum_version;
    if (out_interface != NULL) {
        *out_interface = NULL;
    }
    return FW_STATUS_NOT_FOUND;
}

static const fw_plugin_api_v1 hello_api = {
    sizeof(fw_plugin_api_v1),
    FW_ABI_VERSION_INIT,
    hello_get_descriptor,
    hello_load,
    hello_unload,
    hello_query_interface,
};

FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_hello_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) {
        return NULL;
    }
    return &hello_api;
}
