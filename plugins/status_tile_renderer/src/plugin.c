/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/flutter_zone_profile.h>

#include <string.h>

static const char profile_json[] =
    "{\"schema\":\"facetwire.declarative-zone.v1\",\"type\":\"status-tile\","
    "\"view\":{\"kind\":\"text\",\"field\":\"label\","
    "\"colorField\":\"foreground\"}}";
static const fw_flutter_zone_profile_v1 profile = {
    sizeof(fw_flutter_zone_profile_v1),
    FW_FLUTTER_ZONE_PROFILE_INTERFACE_VERSION,
    FW_STRING_VIEW_LITERAL(profile_json)};
static const fw_capability_descriptor_v1 capability = {
    sizeof(fw_capability_descriptor_v1),
    FW_STRING_VIEW_LITERAL(FW_FLUTTER_ZONE_PROFILE_CAPABILITY_ID),
    FW_STRING_VIEW_LITERAL("facetwire.capability.renderer"), 0u};
static const fw_plugin_descriptor_v1 descriptor = {
    sizeof(fw_plugin_descriptor_v1), FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.reference.status-tile-renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire Status Tile Renderer"),
    FW_STRING_VIEW_LITERAL("FacetWire"),
    FW_STRING_VIEW_LITERAL("0.1.0"), &capability, 1u};

static const fw_plugin_descriptor_v1 *FW_CALL get_descriptor(void) {
    return &descriptor;
}
static fw_status FW_CALL load(const fw_host_api_v1 *host,
                              fw_plugin_handle *out_handle) {
    if (out_handle == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = NULL;
    if (host == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_handle = (fw_plugin_handle)&descriptor;
    return FW_STATUS_OK;
}
static void FW_CALL unload(fw_plugin_handle handle) { (void)handle; }
static fw_status FW_CALL query(fw_plugin_handle handle,
                               fw_string_view interface_id,
                               uint32_t minimum_version,
                               const void **out_interface) {
    const char *expected = FW_FLUTTER_ZONE_PROFILE_INTERFACE_ID;
    if (out_interface == NULL) return FW_STATUS_INVALID_ARGUMENT;
    *out_interface = NULL;
    if (handle != (fw_plugin_handle)&descriptor) return FW_STATUS_INVALID_STATE;
    if (minimum_version > 1u || interface_id.data == NULL ||
        interface_id.length != strlen(expected) ||
        memcmp(interface_id.data, expected, interface_id.length) != 0) {
        return FW_STATUS_NOT_FOUND;
    }
    *out_interface = &profile;
    return FW_STATUS_OK;
}
static const fw_plugin_api_v1 api = {
    sizeof(fw_plugin_api_v1), FW_ABI_VERSION_INIT,
    get_descriptor, load, unload, query};
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested) {
    if (requested.major != FW_ABI_VERSION_MAJOR ||
        requested.minor < FW_ABI_VERSION_MINOR) return NULL;
    return &api;
}

FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_status_tile_renderer_plugin_query(fw_abi_version requested) {
    return facetwire_plugin_query(requested);
}
