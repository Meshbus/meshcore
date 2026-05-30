# MeshCore Support Layer

This directory contains promoted helper and support modules.

Support modules are platform-neutral behavior needed by the generic library,
but they are not top-level protocol-core translations. Examples include packet
manager/table helpers, advert application-data helpers, telemetry encoding
compatibility, and bundled crypto support.

Every support module must remain tied to upstream helper evidence in
`lib/meshcore/ARCHITECTURE.md` or `lib/meshcore/UPSTREAM.md`.

Generic support modules must not allocate through the process heap unless a
future phase explicitly accepts an allocator contract. Packet queue managers
use embedded fixed arenas sized by compile-time macros.
