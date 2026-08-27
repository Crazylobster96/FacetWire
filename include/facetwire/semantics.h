/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_SEMANTICS_H
#define FACETWIRE_SEMANTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared semantic roles. Values already published by the placeholder
 * renderer remain stable; new roles are appended. */
typedef uint32_t fw_semantics_role;
#define FW_SEMANTICS_ROLE_CONTENT_UNAVAILABLE 1u
#define FW_SEMANTICS_ROLE_IMAGE               2u
#define FW_SEMANTICS_ROLE_MEDIA               3u
#define FW_SEMANTICS_ROLE_DOCUMENT            4u
#define FW_SEMANTICS_ROLE_CHART               5u
#define FW_SEMANTICS_ROLE_TEXT                6u

#ifdef __cplusplus
}
#endif

#endif
