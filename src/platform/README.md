# MeshCore Platform Bridge

This directory contains the runtime dispatch bridge for the public platform
hook contract.

`meshcore_platform_bridge.c` dispatches directly to the singleton
`meshcore_platform_*` hook functions declared by
`include/meshcore/platform.h`. The bridge does not own defaults,
install a function table, or call legacy fallback symbols.

New host integration should implement the platform hook header and call the
singleton runtime API from `meshcore/runtime.h`.
