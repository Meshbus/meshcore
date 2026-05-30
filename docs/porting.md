# MeshCore Porting Guide

`include/meshcore` is the complete platform integration contract. A host should
not include headers from `src/`.

## Header Roles

| Header | Role |
| --- | --- |
| `meshcore/platform.h` | Functions the host must implement. |
| `meshcore/runtime.h` | Functions the host may call. |
| `meshcore/types.h` | Shared constants, limits, and public data shapes. |

## Host Responsibilities

- Provide exactly one link-time implementation of every
  `meshcore_platform_*` hook declared by `meshcore/platform.h`.
- Implement unsupported optional behavior as explicit stubs that return a
  negative errno value, `false`, `0`, or no-op according to the hook category.
- Own scheduling, storage, transport, radio hardware, UI, board policy, peer
  records, channel records, and telemetry sources outside the library.
- Treat packet, identity, and channel view pointers as borrowed for the hook
  call only.

## Runtime Use

The runtime is a single process-wide instance. Host code calls
`meshcore_init()`, injects radio/timer events, submits typed requests, and then
calls `meshcore_deinit()` during shutdown.

Callbacks run synchronously from `meshcore_*` entry points. Callback handlers
must not call back into `meshcore_*`; enqueue follow-up work in the host if
needed.
