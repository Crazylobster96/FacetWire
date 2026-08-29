/* SPDX-License-Identifier: MPL-2.0 */
/*
 * The compatibility spike intentionally reuses the production Playground
 * bridge implementation. Keeping one C source of truth prevents the spike,
 * Flutter Native Asset, and visionOS static host from advertising different
 * Flow Layout capability slices.
 */
#include "../../../../examples/placeholder_demo/native/include/facetwire_playground_bridge.h"
#include "../../../../examples/placeholder_demo/native/src/facetwire_playground_bridge.c"
