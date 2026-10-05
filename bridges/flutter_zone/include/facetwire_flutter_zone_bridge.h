/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_FLUTTER_ZONE_BRIDGE_H
#define FACETWIRE_FLUTTER_ZONE_BRIDGE_H

#include <facetwire/facetwire.h>

#if defined(_WIN32)
#  if defined(FW_FLUTTER_ZONE_BRIDGE_BUILDING_LIBRARY)
#    define FW_FLUTTER_ZONE_BRIDGE_API __declspec(dllexport)
#  else
#    define FW_FLUTTER_ZONE_BRIDGE_API __declspec(dllimport)
#  endif
#else
#  define FW_FLUTTER_ZONE_BRIDGE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Host must authorize/pin both this bridge and the exact native library
 * before calling. This is an in-process executable load, not a sandbox.
 * No directory scanning, network, manifest, signature or trust decisions.
 * On failure, *out_length is zero and the caller's bytes are untouched. */
FW_FLUTTER_ZONE_BRIDGE_API fw_status FW_CALL fw_flutter_zone_profile_load(
    fw_string_view absolute_library_path,
    fw_string_view expected_plugin_id,
    unsigned char *out_utf8_json,
    size_t capacity,
    size_t *out_length);

#ifdef __cplusplus
}
#endif
#endif
