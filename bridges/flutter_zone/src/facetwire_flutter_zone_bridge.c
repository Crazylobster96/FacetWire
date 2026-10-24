/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/flutter_zone_profile.h>
#include <facetwire/runtime.h>
#include <facetwire_flutter_zone_bridge.h>

#include <string.h>

static fw_string_view literal(const char *value) {
    fw_string_view view;
    view.data = value;
    view.length = strlen(value);
    return view;
}

static int equal(fw_string_view left, fw_string_view right) {
    return left.length == right.length && left.data != NULL &&
           right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0;
}

fw_status FW_CALL fw_flutter_zone_profile_load(
    fw_string_view absolute_library_path,
    fw_string_view expected_plugin_id,
    unsigned char *out_utf8_json,
    size_t capacity,
    size_t *out_length) {
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    const fw_runtime_config_v1 config = {
        sizeof(fw_runtime_config_v1), &host, 1u};
    const fw_plugin_descriptor_v1 *descriptor = NULL;
    const fw_flutter_zone_profile_v1 *profile;
    const void *interface = NULL;
    fw_runtime *runtime = NULL;
    fw_status status;
    size_t index;
    int advertised = 0;

    if (out_length != NULL) *out_length = 0u;
    if (out_length == NULL || out_utf8_json == NULL || capacity == 0u ||
        absolute_library_path.data == NULL ||
        absolute_library_path.length == 0u ||
        expected_plugin_id.data == NULL || expected_plugin_id.length == 0u) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    status = fw_runtime_create(&config, &runtime);
    if (status != FW_STATUS_OK) return status;
    status = fw_runtime_load_dynamic(runtime, absolute_library_path,
                                     &descriptor);
    if (status != FW_STATUS_OK) goto done;
    if (!equal(descriptor->id, expected_plugin_id)) {
        status = FW_STATUS_INVALID_PLUGIN;
        goto done;
    }
    for (index = 0u; index < descriptor->capability_count; ++index) {
        if (equal(descriptor->capabilities[index].id,
                  literal(FW_FLUTTER_ZONE_PROFILE_CAPABILITY_ID))) {
            advertised = 1;
            break;
        }
    }
    if (!advertised) {
        status = FW_STATUS_UNSUPPORTED;
        goto done;
    }
    status = fw_runtime_query_interface(
        runtime, expected_plugin_id,
        literal(FW_FLUTTER_ZONE_PROFILE_INTERFACE_ID),
        FW_FLUTTER_ZONE_PROFILE_INTERFACE_VERSION, &interface);
    if (status != FW_STATUS_OK) goto done;
    profile = (const fw_flutter_zone_profile_v1 *)interface;
    if (profile == NULL ||
        profile->struct_size < sizeof(fw_flutter_zone_profile_v1) ||
        profile->interface_version != FW_FLUTTER_ZONE_PROFILE_INTERFACE_VERSION ||
        profile->profile_utf8_json.data == NULL ||
        profile->profile_utf8_json.length == 0u) {
        status = FW_STATUS_INVALID_PLUGIN;
        goto done;
    }
    if (profile->profile_utf8_json.length >
        FW_FLUTTER_ZONE_PROFILE_MAX_BYTES) {
        status = FW_STATUS_RESOURCE_LIMIT;
        goto done;
    }
    if (profile->profile_utf8_json.length > capacity) {
        status = FW_STATUS_BUFFER_TOO_SMALL;
        goto done;
    }
    memcpy(out_utf8_json, profile->profile_utf8_json.data,
           profile->profile_utf8_json.length);
    *out_length = profile->profile_utf8_json.length;
    status = FW_STATUS_OK;
done:
    fw_runtime_destroy(runtime);
    return status;
}
