/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_RUNTIME_H
#define FACETWIRE_RUNTIME_H

#include <facetwire/facetwire.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_runtime fw_runtime;

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

FW_API size_t FW_CALL fw_runtime_plugin_count(const fw_runtime *runtime);

FW_API const fw_plugin_descriptor_v1 *FW_CALL
fw_runtime_plugin_at(const fw_runtime *runtime, size_t index);

#ifdef __cplusplus
}
#endif

#endif
