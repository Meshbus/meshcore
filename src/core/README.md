# MeshCore Core Layer

This directory contains the protocol-core MeshCore implementation.

The layer is traceable to top-level `.reference/meshcore/src` protocol
classes. It must stay platform-neutral and must not include Zephyr, Meshbus,
Arduino, board, storage, transport, Bluetooth, shell, or UI headers.

Module ownership follows `lib/meshcore/ARCHITECTURE.md`:

- packet, utils, rng, identity, clock, radio abstraction, dispatcher, mesh,
  and group channel
- protocol-visible callbacks are routed through runtime/port bridges instead
  of directly owning host policy
