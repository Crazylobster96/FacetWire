/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/facetwire.h>

#include <string.h>

static const int dynamic_interface_token = 73;

static int view_equal(fw_string_view value, const char *expected) {
    const size_t length = strlen(expected);
    return value.length == length && value.data != NULL &&
           memcmp(value.data, expected, length) == 0;
}

static const fw_capability_descriptor_v1 capabilities[] = {
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.render"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.renderer"),
        0u,
    },
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.dynamic.only"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.diagnostic"),
        0u,
    },
};

static const fw_plugin_descriptor_v1 descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.test.dynamic"),
    FW_STRING_VIEW_LITERAL("Dynamic contract test plugin"),
    FW_STRING_VIEW_LITERAL("FacetWire"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    capabilities,
    2u,
};

static const fw_plugin_descriptor_v1 *FW_CALL get_descriptor(void) {
    return &descriptor;
}

static fw_status FW_CALL load_plugin(const fw_host_api_v1 *host,
                                     fw_plugin_handle *out_handle) {
    if (out_handle == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_handle = NULL;
    if (host == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = (fw_plugin_handle)&descriptor;
    return FW_STATUS_OK;
}

static void FW_CALL unload_plugin(fw_plugin_handle handle) {
    (void)handle;
}

static fw_status FW_CALL query_interface(
    fw_plugin_handle handle,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface) {
    if (out_interface == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_interface = NULL;
    if (handle != (fw_plugin_handle)&descriptor) {
        return FW_STATUS_INVALID_STATE;
    }
    if (!view_equal(interface_id, "org.facetwire.test.echo/1") ||
        minimum_version > 1u) {
        return FW_STATUS_NOT_FOUND;
    }
    *out_interface = &dynamic_interface_token;
    return FW_STATUS_OK;
}

static const fw_plugin_api_v1 api = {
    sizeof(fw_plugin_api_v1),
    FW_ABI_VERSION_INIT,
    get_descriptor,
    load_plugin,
    unload_plugin,
    query_interface,
};

FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi) {
    if (requested_abi.major != FW_ABI_VERSION_MAJOR ||
        requested_abi.minor < FW_ABI_VERSION_MINOR) {
        return NULL;
    }
    return &api;
}
