/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_RUNTIME_H
#define FACETWIRE_RUNTIME_H

#include <facetwire/facetwire.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_runtime fw_runtime;

typedef enum fw_plugin_source_v1 {
    FW_PLUGIN_SOURCE_UNKNOWN = 0,
    FW_PLUGIN_SOURCE_STATIC = 1,
    FW_PLUGIN_SOURCE_NATIVE_DYNAMIC = 2
} fw_plugin_source_v1;

typedef struct fw_capability_match_v1 {
    uint32_t struct_size;
    size_t plugin_index;
    const fw_plugin_descriptor_v1 *plugin;
    const fw_capability_descriptor_v1 *capability;
    fw_plugin_source_v1 source;
} fw_capability_match_v1;

typedef struct fw_runtime_config_v1 {
    uint32_t struct_size;
    const fw_host_api_v1 *host_api;
    size_t plugin_capacity;
} fw_runtime_config_v1;

FW_API fw_status FW_CALL fw_runtime_create(
    const fw_runtime_config_v1 *config,
    fw_runtime **out_runtime);

FW_API void FW_CALL fw_runtime_destroy(fw_runtime *runtime);

FW_API fw_status FW_CALL fw_runtime_register_static(
    fw_runtime *runtime,
    fw_plugin_query_fn query,
    const fw_plugin_descriptor_v1 **out_descriptor);

/* Loads one explicitly selected native library. The path must be an absolute,
 * UTF-8 path. This API performs no directory scanning, manifest parsing, trust
 * decision, or signature verification; those remain host policy. */
FW_API fw_status FW_CALL fw_runtime_load_dynamic(
    fw_runtime *runtime,
    fw_string_view absolute_library_path,
    const fw_plugin_descriptor_v1 **out_descriptor);

/* Unloads a plugin previously loaded by fw_runtime_load_dynamic. Descriptor,
 * capability, and interface pointers obtained from that plugin become invalid.
 * Statically registered plugins return FW_STATUS_UNSUPPORTED. */
FW_API fw_status FW_CALL fw_runtime_unload_dynamic(
    fw_runtime *runtime,
    fw_string_view plugin_id);

FW_API size_t FW_CALL fw_runtime_plugin_count(const fw_runtime *runtime);

FW_API const fw_plugin_descriptor_v1 *FW_CALL
fw_runtime_plugin_at(const fw_runtime *runtime, size_t index);

FW_API fw_plugin_source_v1 FW_CALL
fw_runtime_plugin_source_at(const fw_runtime *runtime, size_t index);

FW_API fw_status FW_CALL fw_runtime_find_plugin(
    const fw_runtime *runtime,
    fw_string_view plugin_id,
    size_t *out_plugin_index,
    const fw_plugin_descriptor_v1 **out_descriptor);

/* Enumerates providers in stable registration order. Pass start_plugin_index=0
 * for the first provider and previous_match.plugin_index + 1 for the next. */
FW_API fw_status FW_CALL fw_runtime_find_capability(
    const fw_runtime *runtime,
    fw_string_view capability_id,
    size_t start_plugin_index,
    fw_capability_match_v1 *out_match);

/* preferred_plugin_id may be empty. An empty preference selects the first
 * registered provider; a non-empty preference must name a matching provider. */
FW_API fw_status FW_CALL fw_runtime_select_capability(
    const fw_runtime *runtime,
    fw_string_view capability_id,
    fw_string_view preferred_plugin_id,
    fw_capability_match_v1 *out_match);

FW_API fw_status FW_CALL fw_runtime_query_interface(
    fw_runtime *runtime,
    fw_string_view plugin_id,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface);

#ifdef __cplusplus
}
#endif

#endif
