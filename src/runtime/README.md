# MeshCore Runtime Layer

This directory contains the Layer 2 host-driven MeshCore runtime.

The runtime composes `src/core`, `src/support`, and `src/platform`, then
exposes the public C ABI from `include/meshcore/runtime.h`. Hosts
own scheduling, storage, transport, and platform integration through the direct
`meshcore_platform_*` hooks declared by `meshcore/platform.h`.

The runtime must stay a deterministic event pump. Do not introduce hidden
threads, Zephyr workqueues, Meshbus state, Arduino `loop()` scheduling, or
host-specific storage here.

The public ABI is currently a singleton facade. Internal state is accessed
through the private current-context helper so a future context API can be added
without spreading global state through runtime files.
