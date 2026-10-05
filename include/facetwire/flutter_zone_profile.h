/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_FLUTTER_ZONE_PROFILE_H
#define FACETWIRE_FLUTTER_ZONE_PROFILE_H

#include <facetwire/facetwire.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional plugin interface. The immutable UTF-8 JSON profile is borrowed
 * until the plugin unloads; it uses facetwire.declarative-zone.v1 and must be
 * parsed and validated by the Flutter host before accepting any document. */
#define FW_FLUTTER_ZONE_PROFILE_INTERFACE_ID "facetwire.renderer.flutter-zone.v1"
#define FW_FLUTTER_ZONE_PROFILE_CAPABILITY_ID "facetwire.renderer.flutter-zone"
#define FW_FLUTTER_ZONE_PROFILE_INTERFACE_VERSION 1u
#define FW_FLUTTER_ZONE_PROFILE_MAX_BYTES 65536u

typedef struct fw_flutter_zone_profile_v1 {
    uint32_t struct_size;
    uint32_t interface_version;
    fw_string_view profile_utf8_json;
} fw_flutter_zone_profile_v1;

#ifdef __cplusplus
}
#endif
#endif
