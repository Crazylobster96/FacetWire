/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/runtime.h>
#include "dynamic_library.h"

#include <stdlib.h>
#include <string.h>

#define FW_DEFAULT_PLUGIN_CAPACITY 64u

typedef struct fw_plugin_entry {
    const fw_plugin_api_v1 *api;
    const fw_plugin_descriptor_v1 *descriptor;
    fw_plugin_handle handle;
    fw_plugin_source_v1 source;
    fw_native_library library;
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
    size_t previous;

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
        for (previous = 0u; previous < index; ++previous) {
            if (fw_string_equal(
                    descriptor->capabilities[previous].id,
                    capability->id)) {
                return FW_STATUS_INVALID_PLUGIN;
            }
        }
    }
    return FW_STATUS_OK;
}

static void fw_release_entry(fw_plugin_entry *entry) {
    if (entry->api != NULL && entry->api->unload != NULL) {
        entry->api->unload(entry->handle);
    }
    fw_native_library_close(&entry->library);
    memset(entry, 0, sizeof(*entry));
}

static fw_status fw_register_plugin(
    fw_runtime *runtime,
    fw_plugin_query_fn query,
    fw_plugin_source_v1 source,
    fw_native_library library,
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
        if (handle != NULL && api->unload != NULL) {
            api->unload(handle);
        }
        return FW_STATUS_PLUGIN_ERROR;
    }

    runtime->plugins[runtime->plugin_count].api = api;
    runtime->plugins[runtime->plugin_count].descriptor = descriptor;
    runtime->plugins[runtime->plugin_count].handle = handle;
    runtime->plugins[runtime->plugin_count].source = source;
    runtime->plugins[runtime->plugin_count].library = library;
    ++runtime->plugin_count;

    if (out_descriptor != NULL) {
        *out_descriptor = descriptor;
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
        fw_release_entry(entry);
    }
    free(runtime->plugins);
    free(runtime);
}

fw_status FW_CALL fw_runtime_register_static(
    fw_runtime *runtime,
    fw_plugin_query_fn query,
    const fw_plugin_descriptor_v1 **out_descriptor) {
    const fw_native_library library = {NULL};
    return fw_register_plugin(runtime, query, FW_PLUGIN_SOURCE_STATIC,
                              library, out_descriptor);
}

fw_status FW_CALL fw_runtime_load_dynamic(
    fw_runtime *runtime,
    fw_string_view absolute_library_path,
    const fw_plugin_descriptor_v1 **out_descriptor) {
    fw_native_library library = {NULL};
    fw_plugin_query_fn query = NULL;
    fw_status status;

    if (out_descriptor != NULL) {
        *out_descriptor = NULL;
    }
    if (runtime == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    status = fw_native_library_open(absolute_library_path, &library, &query);
    if (status != FW_STATUS_OK) {
        return status;
    }
    status = fw_register_plugin(runtime, query,
        FW_PLUGIN_SOURCE_NATIVE_DYNAMIC, library, out_descriptor);
    if (status != FW_STATUS_OK) {
        fw_native_library_close(&library);
    }
    return status;
}

fw_status FW_CALL fw_runtime_unload_dynamic(
    fw_runtime *runtime,
    fw_string_view plugin_id) {
    size_t index;
    fw_status status;

    if (runtime == NULL || !fw_string_is_valid(plugin_id) ||
        plugin_id.length == 0u) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    status = fw_runtime_find_plugin(runtime, plugin_id, &index, NULL);
    if (status != FW_STATUS_OK) {
        return status;
    }
    if (runtime->plugins[index].source !=
        FW_PLUGIN_SOURCE_NATIVE_DYNAMIC) {
        return FW_STATUS_UNSUPPORTED;
    }
    fw_release_entry(&runtime->plugins[index]);
    if (index + 1u < runtime->plugin_count) {
        memmove(&runtime->plugins[index], &runtime->plugins[index + 1u],
                (runtime->plugin_count - index - 1u) *
                    sizeof(*runtime->plugins));
    }
    --runtime->plugin_count;
    memset(&runtime->plugins[runtime->plugin_count], 0,
           sizeof(*runtime->plugins));
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

fw_plugin_source_v1 FW_CALL
fw_runtime_plugin_source_at(const fw_runtime *runtime, size_t index) {
    if (runtime == NULL || index >= runtime->plugin_count) {
        return FW_PLUGIN_SOURCE_UNKNOWN;
    }
    return runtime->plugins[index].source;
}

fw_status FW_CALL fw_runtime_find_plugin(
    const fw_runtime *runtime,
    fw_string_view plugin_id,
    size_t *out_plugin_index,
    const fw_plugin_descriptor_v1 **out_descriptor) {
    size_t index;

    if (out_plugin_index != NULL) {
        *out_plugin_index = 0u;
    }
    if (out_descriptor != NULL) {
        *out_descriptor = NULL;
    }
    if (runtime == NULL || !fw_string_is_valid(plugin_id) ||
        plugin_id.length == 0u) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < runtime->plugin_count; ++index) {
        if (fw_string_equal(runtime->plugins[index].descriptor->id,
                            plugin_id)) {
            if (out_plugin_index != NULL) {
                *out_plugin_index = index;
            }
            if (out_descriptor != NULL) {
                *out_descriptor = runtime->plugins[index].descriptor;
            }
            return FW_STATUS_OK;
        }
    }
    return FW_STATUS_NOT_FOUND;
}

fw_status FW_CALL fw_runtime_find_capability(
    const fw_runtime *runtime,
    fw_string_view capability_id,
    size_t start_plugin_index,
    fw_capability_match_v1 *out_match) {
    size_t plugin_index;
    uint32_t match_size;

    if (out_match == NULL ||
        out_match->struct_size < sizeof(fw_capability_match_v1) ||
        runtime == NULL || !fw_string_is_valid(capability_id) ||
        capability_id.length == 0u) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    match_size = out_match->struct_size;
    memset(out_match, 0, sizeof(*out_match));
    out_match->struct_size = match_size;

    for (plugin_index = start_plugin_index;
         plugin_index < runtime->plugin_count; ++plugin_index) {
        const fw_plugin_entry *entry = &runtime->plugins[plugin_index];
        size_t capability_index;
        for (capability_index = 0u;
             capability_index < entry->descriptor->capability_count;
             ++capability_index) {
            const fw_capability_descriptor_v1 *capability =
                &entry->descriptor->capabilities[capability_index];
            if (fw_string_equal(capability->id, capability_id)) {
                out_match->plugin_index = plugin_index;
                out_match->plugin = entry->descriptor;
                out_match->capability = capability;
                out_match->source = entry->source;
                return FW_STATUS_OK;
            }
        }
    }
    return FW_STATUS_NOT_FOUND;
}

fw_status FW_CALL fw_runtime_select_capability(
    const fw_runtime *runtime,
    fw_string_view capability_id,
    fw_string_view preferred_plugin_id,
    fw_capability_match_v1 *out_match) {
    size_t plugin_index;
    size_t capability_index;
    uint32_t match_size;
    fw_status status;

    if (runtime == NULL || out_match == NULL ||
        out_match->struct_size < sizeof(fw_capability_match_v1) ||
        !fw_string_is_valid(capability_id) || capability_id.length == 0u ||
        !fw_string_is_valid(preferred_plugin_id)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    match_size = out_match->struct_size;
    memset(out_match, 0, sizeof(*out_match));
    out_match->struct_size = match_size;
    if (preferred_plugin_id.length == 0u) {
        return fw_runtime_find_capability(runtime, capability_id, 0u,
                                          out_match);
    }
    status = fw_runtime_find_plugin(runtime, preferred_plugin_id,
                                    &plugin_index, NULL);
    if (status != FW_STATUS_OK) {
        return status;
    }
    for (capability_index = 0u;
         capability_index <
             runtime->plugins[plugin_index].descriptor->capability_count;
         ++capability_index) {
        const fw_capability_descriptor_v1 *capability =
            &runtime->plugins[plugin_index]
                 .descriptor->capabilities[capability_index];
        if (fw_string_equal(capability->id, capability_id)) {
            out_match->plugin_index = plugin_index;
            out_match->plugin = runtime->plugins[plugin_index].descriptor;
            out_match->capability = capability;
            out_match->source = runtime->plugins[plugin_index].source;
            return FW_STATUS_OK;
        }
    }
    return FW_STATUS_NOT_FOUND;
}

fw_status FW_CALL fw_runtime_query_interface(
    fw_runtime *runtime,
    fw_string_view plugin_id,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface) {
    size_t plugin_index;
    fw_status status;

    if (out_interface == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_interface = NULL;
    if (runtime == NULL || !fw_string_is_valid(interface_id) ||
        interface_id.length == 0u) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    status = fw_runtime_find_plugin(runtime, plugin_id,
                                    &plugin_index, NULL);
    if (status != FW_STATUS_OK) {
        return status;
    }
    if (runtime->plugins[plugin_index].api->query_interface == NULL) {
        return FW_STATUS_NOT_FOUND;
    }
    status = runtime->plugins[plugin_index].api->query_interface(
        runtime->plugins[plugin_index].handle, interface_id,
        minimum_version, out_interface);
    if (status != FW_STATUS_OK) {
        *out_interface = NULL;
    }
    return status;
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
