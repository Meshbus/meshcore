# MeshCore Public Include Contract

This directory is the complete public boundary for platform integration. New
platform code should be able to integrate MeshCore by reading only the
headers in this directory.

## Header Roles

| Header | Platform role |
| --- | --- |
| `meshcore/platform.h` | Host-implemented singleton platform hook contract. The platform must provide the declared `meshcore_platform_*` functions. |
| `meshcore/runtime.h` | Host-callable API. Platform or service code calls lifecycle, radio RX/TX injection, timer injection, and typed request functions here. |
| `meshcore/types.h` | Shared ABI types, constants, limits, and event data shapes used by both `platform.h` and `runtime.h`. |

There are no flat compatibility headers in this boundary. Include the canonical
nested header that matches the role being implemented or called.

## Platform Implements

Platform integration implements the direct hook functions declared by
`meshcore/platform.h`.

There is one global MeshCore runtime instance. Hooks are link-time singleton
symbols, not runtime-installed function tables. Unsupported optional features
should be implemented as explicit stubs that return `-ENOSYS`, `false`, `0`,
or no-op according to the hook category.

There are no weak fallback host symbols. Missing required hook definitions
should fail at link time.

Protocol receive and policy hooks receive public read-only view types from
`meshcore/types.h`, such as `meshcore_common_packet_view_t`. These views are
borrowed for the duration of the hook call and intentionally hide core-private
objects such as packets, identities, and group channels.

## Platform Calls

Platform or service code calls only the public functions declared by
`meshcore/runtime.h`:

- initialize and deinitialize the singleton runtime
- inject radio RX frames
- inject radio TX completion
- inject timer expiry
- request local advert, peer advert replay, peer messages, channel messages,
  path discovery, trace, telemetry, binary request, raw data, and control data

Platform code must not call private functions under `src/`.

## Feature Dependencies

Every full-runtime host links all hook symbols. Feature support is controlled by
what those hooks do:

| Feature area | Host hook category |
| --- | --- |
| Runtime scheduling | `meshcore_platform_timer_*` |
| Real radio operation | `meshcore_platform_radio_*` |
| Local identity, adverts, and request handling | `meshcore_platform_node_*` |
| Peer direct send, path lookup, and peer receive handling | `meshcore_platform_peer_*` |
| Channel/group messages and channel datagrams | `meshcore_platform_channel_*` |
| Packet cryptography | `meshcore_platform_crypto_*` plus host-owned channel/peer secrets |
| Runtime event publication | `meshcore_platform_event_*` and `meshcore_platform_mesh_on_*` |
| Dispatcher and mesh policy tuning | `meshcore_platform_dispatcher_*` and `meshcore_platform_mesh_*` |
| Telemetry response handling | `meshcore_platform_telemetry_*` |

Host business objects such as application messages, contacts, channels, settings,
transports, storage, UI, and board state stay outside this library.
