/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_FACETWIRE_H
#define FACETWIRE_FACETWIRE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define FW_CALL __cdecl
#if defined(FACETWIRE_SHARED)
#if defined(FACETWIRE_BUILDING_LIBRARY)
#define FW_API __declspec(dllexport)
#else
#define FW_API __declspec(dllimport)
#endif
#else
#define FW_API
#endif
#define FW_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FW_CALL
#if defined(__GNUC__) || defined(__clang__)
#define FW_API __attribute__((visibility("default")))
#define FW_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FW_API
#define FW_PLUGIN_EXPORT
#endif
#endif

#define FW_ABI_VERSION_MAJOR 1u
#define FW_ABI_VERSION_MINOR 0u
#define FW_PLUGIN_QUERY_SYMBOL "facetwire_plugin_query"

typedef struct fw_abi_version {
    uint16_t major;
    uint16_t minor;
} fw_abi_version;

#define FW_ABI_VERSION_INIT { FW_ABI_VERSION_MAJOR, FW_ABI_VERSION_MINOR }

static inline fw_abi_version fw_abi_version_current(void) {
    const fw_abi_version version = FW_ABI_VERSION_INIT;
    return version;
}

#define FW_ABI_VERSION_CURRENT fw_abi_version_current()

typedef struct fw_string_view {
    const char *data;
    size_t length;
} fw_string_view;

#define FW_STRING_VIEW_LITERAL(value)                                        \
    { (value), sizeof(value) - 1u }

typedef enum fw_status {
    FW_STATUS_OK = 0,
    FW_STATUS_INVALID_ARGUMENT = 1,
    FW_STATUS_INCOMPATIBLE_ABI = 2,
    FW_STATUS_INVALID_PLUGIN = 3,
    FW_STATUS_ALREADY_REGISTERED = 4,
    FW_STATUS_CAPACITY_EXCEEDED = 5,
    FW_STATUS_OUT_OF_MEMORY = 6,
    FW_STATUS_NOT_FOUND = 7,
    FW_STATUS_PLUGIN_ERROR = 8,
    FW_STATUS_BUFFER_TOO_SMALL = 9,
    FW_STATUS_CANCELLED = 10,
    FW_STATUS_UNSUPPORTED = 11,
    FW_STATUS_INVALID_STATE = 12,
    FW_STATUS_RESOURCE_LIMIT = 13,
    FW_STATUS_SINK_REJECTED = 14
} fw_status;

typedef enum fw_log_level {
    FW_LOG_TRACE = 0,
    FW_LOG_DEBUG = 1,
    FW_LOG_INFO = 2,
    FW_LOG_WARNING = 3,
    FW_LOG_ERROR = 4
} fw_log_level;

typedef void(FW_CALL *fw_log_fn)(void *user_data,
                                 fw_log_level level,
                                 fw_string_view target,
                                 fw_string_view message);

typedef struct fw_host_api_v1 {
    uint32_t struct_size;
    fw_abi_version abi_version;
    void *user_data;
    fw_log_fn log;
} fw_host_api_v1;

typedef struct fw_capability_descriptor_v1 {
    uint32_t struct_size;
    fw_string_view id;
    fw_string_view kind;
    uint64_t flags;
} fw_capability_descriptor_v1;

typedef struct fw_plugin_descriptor_v1 {
    uint32_t struct_size;
    fw_abi_version abi_version;
    fw_string_view id;
    fw_string_view name;
    fw_string_view vendor;
    fw_string_view version;
    const fw_capability_descriptor_v1 *capabilities;
    size_t capability_count;
} fw_plugin_descriptor_v1;

typedef void *fw_plugin_handle;

typedef const fw_plugin_descriptor_v1 *(FW_CALL *fw_get_descriptor_fn)(void);
typedef fw_status(FW_CALL *fw_plugin_load_fn)(const fw_host_api_v1 *host,
                                              fw_plugin_handle *out_handle);
typedef void(FW_CALL *fw_plugin_unload_fn)(fw_plugin_handle handle);
typedef fw_status(FW_CALL *fw_query_interface_fn)(
    fw_plugin_handle handle,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface);

typedef struct fw_plugin_api_v1 {
    uint32_t struct_size;
    fw_abi_version abi_version;
    fw_get_descriptor_fn get_descriptor;
    fw_plugin_load_fn load;
    fw_plugin_unload_fn unload;
    fw_query_interface_fn query_interface;
} fw_plugin_api_v1;

typedef const fw_plugin_api_v1 *(FW_CALL *fw_plugin_query_fn)(
    fw_abi_version requested_abi);

/* A dynamic plugin exports this exact symbol. Statically linked plugins may
 * pass any equivalent query function directly to the runtime. */
FW_PLUGIN_EXPORT const fw_plugin_api_v1 *FW_CALL
facetwire_plugin_query(fw_abi_version requested_abi);

FW_API const char *FW_CALL fw_status_name(fw_status status);

#ifdef __cplusplus
}
#endif

#endif
