/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_DYNAMIC_LIBRARY_H
#define FACETWIRE_DYNAMIC_LIBRARY_H

#include <facetwire/facetwire.h>

typedef struct fw_native_library {
    void *handle;
} fw_native_library;

fw_status fw_native_library_open(fw_string_view absolute_path,
                                 fw_native_library *out_library,
                                 fw_plugin_query_fn *out_query);

void fw_native_library_close(fw_native_library *library);

#endif
