/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/runtime.h>

#include <stdio.h>

static void FW_CALL print_log(void *user_data,
                              fw_log_level level,
                              fw_string_view target,
                              fw_string_view message) {
    (void)user_data;
    (void)level;
    printf("[%.*s] %.*s\n",
           (int)target.length,
           target.data,
           (int)message.length,
           message.data);
}

int main(void) {
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1),
        FW_ABI_VERSION_INIT,
        NULL,
        print_log,
    };
    const fw_runtime_config_v1 config = {
        sizeof(fw_runtime_config_v1),
        &host,
        8u,
    };
    const fw_plugin_descriptor_v1 *descriptor = NULL;
    fw_runtime *runtime = NULL;
    fw_status status;

    status = fw_runtime_create(&config, &runtime);
    if (status != FW_STATUS_OK) {
        fprintf(stderr, "runtime creation failed: %s\n", fw_status_name(status));
        return 1;
    }

    status = fw_runtime_register_static(
        runtime, facetwire_plugin_query, &descriptor);
    if (status != FW_STATUS_OK) {
        fprintf(stderr, "plugin registration failed: %s\n",
                fw_status_name(status));
        fw_runtime_destroy(runtime);
        return 1;
    }

    printf("registered %zu plugin: %.*s\n",
           fw_runtime_plugin_count(runtime),
           (int)descriptor->name.length,
           descriptor->name.data);
    fw_runtime_destroy(runtime);
    return 0;
}
