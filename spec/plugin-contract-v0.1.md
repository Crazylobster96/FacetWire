# FacetWire Plugin Contract 0.1

Status: **Draft**
Normative terms: MUST, MUST NOT, SHOULD, SHOULD NOT, and MAY are interpreted as
requirements for conforming implementations.

## 1. Purpose

This document specifies the base contract between a FacetWire host and plugin.
Version 0.1 covers identity, capability declarations, ABI negotiation,
lifecycle, logging, static registration, and interface discovery.

### Completeness check

The contract deliberately excludes rendering commands and file manifests until
their behavior and resource limits are separately specified.

## 2. Identity

A plugin MUST expose a non-empty, globally stable UTF-8 identifier. Reverse-DNS
form is RECOMMENDED. The identifier is a compatibility identity and MUST NOT be
reused for an unrelated implementation.

A descriptor MUST contain an ABI version, display name, vendor, plugin version,
and zero or more capability descriptors. Capability identifiers MUST be unique
inside the plugin.

### Completeness check

Stable machine identity is separated from localized display text and release
versioning.

## 3. ABI negotiation

The host requests an ABI version through a plugin query function. The plugin
MUST return `NULL` when it cannot implement the requested ABI major version.
The runtime accepts descriptors and APIs whose major version equals the host
major and whose minor version does not exceed the host minor.

All structures intended for extension MUST begin with a `uint32_t struct_size`.
New fields MUST be appended. A reader MUST NOT access fields beyond the supplied
size. Version 0.1 currently requires the complete v1 structure size.

### Completeness check

Major mismatch is explicit; additive minor evolution is size-gated; field
reordering and implicit compiler layout changes are prohibited.

## 4. Lifetime and ownership

The host registers a query function, validates returned immutable tables, and
calls `load` once. On success it calls `unload` exactly once before releasing
the plugin module or host services. Unload occurs in reverse registration order.

Plugin descriptors and function tables MUST remain valid from query through
unload. A plugin MUST NOT retain the host API after unload. Neither party may
free memory allocated by the other unless a future interface provides an
explicit paired release callback.

### Completeness check

The lifecycle has balanced operations, deterministic order, and an allocator-
safe ownership rule.

## 5. Capability and interface discovery

Capability descriptors advertise coarse functions for selection and policy.
Callable capability interfaces are versioned function tables returned by
`query_interface`. An unknown interface MUST return `FW_STATUS_NOT_FOUND` and
set the output pointer to `NULL` when it is provided.

An interface specification MUST define threading, reentrancy, ownership,
cancellation, error handling, resource limits, and lifetime before it is stable.

### Completeness check

Advertising a capability does not expose platform objects or bypass the
versioned callable interface.

## 6. Deployment profiles

A native dynamic plugin MUST export `facetwire_plugin_query` with C linkage and
the calling convention defined in `facetwire.h`. A statically linked plugin MAY
use a uniquely named query function and register its pointer directly. IPC,
Wasm, and remote adapters MAY represent the contract through serialization but
MUST preserve observable identity, lifecycle, status, and capability semantics.

### Completeness check

The same source-facing contract supports desktop dynamic loading and restricted
platform static registration without claiming binary interchangeability.

## 7. Errors and diagnostics

Base operations return `fw_status`. Status values are stable numeric protocol
values and MUST NOT be renumbered. Human-readable status names are diagnostic,
not localization keys. Plugins SHOULD use the host log callback when available
and MUST operate correctly when it is absent.

### Completeness check

Machine-readable status remains separate from optional logging and user-facing
localization.

## 8. Conformance

A conforming 0.1 runtime MUST reject incompatible ABIs and malformed
descriptors, prevent duplicate plugin identities, enforce capacity, balance
load/unload, and expose deterministic registration order. A conforming plugin
MUST pass the public header build and lifecycle tests on every claimed platform.

### Completeness check

The requirements map to executable tests rather than documentation-only claims.
