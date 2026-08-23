/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/runtime.h>

#include <stdlib.h>
#include <string.h>

#define FW_DEFAULT_PLUGIN_CAPACITY 64u

typedef struct fw_plugin_entry {
    const fw_plugin_api_v1 *api;
    const fw_plugin_descriptor_v1 *descriptor;
    fw_plugin_handle handle;
} fw_plugin_entry;

struct fw_runtime {
    fw_host_api_v1 host_api;
    fw_plugin_entry *plugins;
    size_t plugin_count;
    size_t plugin_capacity;
};

static int fw_string_is_valid(fw_string_view value) {
    return value.length == 0u || value.data != NULL;
}

static int fw_string_equal(fw_string_view left, fw_string_view right) {
    if (left.length != right.length) {
        return 0;
    }
    if (left.length == 0u) {
        return 1;
    }
    return memcmp(left.data, right.data, left.length) == 0;
}

static int fw_abi_is_compatible(fw_abi_version version) {
    return version.major == FW_ABI_VERSION_MAJOR &&
           version.minor <= FW_ABI_VERSION_MINOR;
}

static fw_status fw_validate_descriptor(
    const fw_plugin_descriptor_v1 *descriptor) {
    size_t index;

    if (descriptor == NULL ||
        descriptor->struct_size < sizeof(fw_plugin_descriptor_v1) ||
        !fw_abi_is_compatible(descriptor->abi_version) ||
        !fw_string_is_valid(descriptor->id) || descriptor->id.length == 0u ||
        !fw_string_is_valid(descriptor->name) ||
        !fw_string_is_valid(descriptor->vendor) ||
        !fw_string_is_valid(descriptor->version)) {
        return FW_STATUS_INVALID_PLUGIN;
    }
    if (descriptor->capability_count > 0u &&
        descriptor->capabilities == NULL) {
        return FW_STATUS_INVALID_PLUGIN;
    }
    for (index = 0u; index < descriptor->capability_count; ++index) {
        const fw_capability_descriptor_v1 *capability =
            &descriptor->capabilities[index];
        if (capability->struct_size <
                sizeof(fw_capability_descriptor_v1) ||
            !fw_string_is_valid(capability->id) ||
            capability->id.length == 0u ||
            !fw_string_is_valid(capability->kind) ||
            capability->kind.length == 0u) {
            return FW_STATUS_INVALID_PLUGIN;
        }
    }
    return FW_STATUS_OK;
}

fw_status FW_CALL fw_runtime_create(const fw_runtime_config_v1 *config,
                                    fw_runtime **out_runtime) {
    fw_runtime *runtime;
    size_t capacity;

    if (out_runtime == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;

    if (config == NULL ||
        config->struct_size < sizeof(fw_runtime_config_v1) ||
        config->host_api == NULL ||
        config->host_api->struct_size < sizeof(fw_host_api_v1) ||
        !fw_abi_is_compatible(config->host_api->abi_version)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }

    capacity = config->plugin_capacity;
    if (capacity == 0u) {
        capacity = FW_DEFAULT_PLUGIN_CAPACITY;
    }

    runtime = (fw_runtime *)calloc(1u, sizeof(*runtime));
    if (runtime == NULL) {
        return FW_STATUS_OUT_OF_MEMORY;
    }
    runtime->plugins =
        (fw_plugin_entry *)calloc(capacity, sizeof(*runtime->plugins));
    if (runtime->plugins == NULL) {
        free(runtime);
        return FW_STATUS_OUT_OF_MEMORY;
    }

    runtime->host_api = *config->host_api;
    runtime->plugin_capacity = capacity;
    *out_runtime = runtime;
    return FW_STATUS_OK;
}

void FW_CALL fw_runtime_destroy(fw_runtime *runtime) {
    size_t index;

    if (runtime == NULL) {
        return;
    }
    for (index = runtime->plugin_count; index > 0u; --index) {
        fw_plugin_entry *entry = &runtime->plugins[index - 1u];
        if (entry->api->unload != NULL) {
            entry->api->unload(entry->handle);
        }
    }
    free(runtime->plugins);
    free(runtime);
}

fw_status FW_CALL fw_runtime_register_static(
    fw_runtime *runtime,
    fw_plugin_query_fn query,
    const fw_plugin_descriptor_v1 **out_descriptor) {
    const fw_plugin_api_v1 *api;
    const fw_plugin_descriptor_v1 *descriptor;
    fw_plugin_handle handle = NULL;
    fw_status status;
    size_t index;

    if (out_descriptor != NULL) {
        *out_descriptor = NULL;
    }
    if (runtime == NULL || query == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    if (runtime->plugin_count >= runtime->plugin_capacity) {
        return FW_STATUS_CAPACITY_EXCEEDED;
    }

    api = query(FW_ABI_VERSION_CURRENT);
    if (api == NULL || api->struct_size < sizeof(fw_plugin_api_v1) ||
        !fw_abi_is_compatible(api->abi_version) ||
        api->get_descriptor == NULL || api->load == NULL) {
        return FW_STATUS_INCOMPATIBLE_ABI;
    }

    descriptor = api->get_descriptor();
    status = fw_validate_descriptor(descriptor);
    if (status != FW_STATUS_OK) {
        return status;
    }

    for (index = 0u; index < runtime->plugin_count; ++index) {
        if (fw_string_equal(runtime->plugins[index].descriptor->id,
                            descriptor->id)) {
            return FW_STATUS_ALREADY_REGISTERED;
        }
    }

    status = api->load(&runtime->host_api, &handle);
    if (status != FW_STATUS_OK) {
        return FW_STATUS_PLUGIN_ERROR;
    }

    runtime->plugins[runtime->plugin_count].api = api;
    runtime->plugins[runtime->plugin_count].descriptor = descriptor;
    runtime->plugins[runtime->plugin_count].handle = handle;
    ++runtime->plugin_count;

    if (out_descriptor != NULL) {
        *out_descriptor = descriptor;
    }
    return FW_STATUS_OK;
}

size_t FW_CALL fw_runtime_plugin_count(const fw_runtime *runtime) {
    return runtime == NULL ? 0u : runtime->plugin_count;
}

const fw_plugin_descriptor_v1 *FW_CALL
fw_runtime_plugin_at(const fw_runtime *runtime, size_t index) {
    if (runtime == NULL || index >= runtime->plugin_count) {
        return NULL;
    }
    return runtime->plugins[index].descriptor;
}

const char *FW_CALL fw_status_name(fw_status status) {
    switch (status) {
    case FW_STATUS_OK:
        return "ok";
    case FW_STATUS_INVALID_ARGUMENT:
        return "invalid_argument";
    case FW_STATUS_INCOMPATIBLE_ABI:
        return "incompatible_abi";
    case FW_STATUS_INVALID_PLUGIN:
        return "invalid_plugin";
    case FW_STATUS_ALREADY_REGISTERED:
        return "already_registered";
    case FW_STATUS_CAPACITY_EXCEEDED:
        return "capacity_exceeded";
    case FW_STATUS_OUT_OF_MEMORY:
        return "out_of_memory";
    case FW_STATUS_NOT_FOUND:
        return "not_found";
    case FW_STATUS_PLUGIN_ERROR:
        return "plugin_error";
    case FW_STATUS_BUFFER_TOO_SMALL:
        return "buffer_too_small";
    case FW_STATUS_CANCELLED:
        return "cancelled";
    case FW_STATUS_UNSUPPORTED:
        return "unsupported";
    case FW_STATUS_INVALID_STATE:
        return "invalid_state";
    case FW_STATUS_RESOURCE_LIMIT:
        return "resource_limit";
    case FW_STATUS_SINK_REJECTED:
        return "sink_rejected";
    default:
        return "unknown";
    }
}
