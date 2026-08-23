/* SPDX-License-Identifier: MPL-2.0 */
#ifndef FACETWIRE_RENDERER_H
#define FACETWIRE_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_RENDERER_CAPABILITY_KIND "facetwire.capability.renderer"

typedef uint32_t fw_renderer_flags;
#define FW_RENDERER_FLAG_NONE          0u
#define FW_RENDERER_FLAG_DETERMINISTIC (1u << 0)
#define FW_RENDERER_FLAG_HEADLESS      (1u << 1)
#define FW_RENDERER_FLAG_SEMANTICS     (1u << 2)
#define FW_RENDERER_FLAG_HIT_TEST      (1u << 3)

#ifdef __cplusplus
}
#endif

#endif
