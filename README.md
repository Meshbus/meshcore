# MeshCore C Library

`meshcore` is a platform-neutral C MeshCore protocol/runtime library. It keeps
LoRa wire behavior traceable to upstream Arduino MeshCore while leaving
scheduling, storage, transport, radio hardware, UI, and board policy to the
host.

## Public Boundary

Hosts integrate through the headers under `include/meshcore`:

| Header | Role |
| --- | --- |
| `meshcore/platform.h` | Functions the host must implement for platform hooks, host-owned storage lookup, policy, and event publication. |
| `meshcore/runtime.h` | Functions the host calls to initialize the singleton runtime, inject radio/timer events, and submit typed requests. |
| `meshcore/types.h` | Shared ABI constants and public data/view types. |

Headers under `src/` are private implementation details and are not installed
or consumed by platform code.

## Standalone Build

The library and examples can be built without Zephyr, west, or Twister:

```sh
cmake -S . -B build.meshcore-native \
  -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build build.meshcore-native
ctest --test-dir build.meshcore-native --output-on-failure
```

`MESHCORE_BUILD_TESTS=ON` builds native CTest tests with a deterministic fake
platform implementation. These tests validate the public headers, selected
packet/core behavior, and that the full runtime links against direct
`meshcore_platform_*` hooks.

## CMake Consumption

Use the library directly from a source checkout:

```cmake
add_subdirectory(path/to/meshcore)
target_link_libraries(app PRIVATE meshcore::meshcore)
```

Or install and consume it as a CMake package:

```sh
cmake -S . -B build.meshcore-install -DMESHCORE_INSTALL=ON
cmake --build build.meshcore-install
cmake --install build.meshcore-install --prefix /tmp/meshcore-prefix
```

```cmake
find_package(meshcore CONFIG REQUIRED)
target_link_libraries(app PRIVATE meshcore::meshcore)
```

`MESHCORE_INSTALL` and `MESHCORE_BUILD_EXAMPLES` default to enabled only when
`meshcore` is the top-level CMake project. They default to disabled when the
library is added as a subproject.

## Examples

`examples/minimal_host` demonstrates the smallest standalone host shape:

- implement the `meshcore_platform_*` hook symbols declared by
  `meshcore/platform.h`;
- call the singleton runtime API declared by `meshcore/runtime.h`;
- link only against `meshcore::meshcore`.

## Testing Model

Native tests are the independent library test path. They use CMake + CTest and
small host fakes instead of Zephyr `ztest`, west, or Twister.

Host integration tests remain useful for platform-specific adapters, but they
are not the only way to validate the library:

- native CTest: platform-neutral ABI, core, support, runtime, and fake-host
  behavior;
- upstream sync tools: source manifest and reference evidence drift checks;
- downstream host tests: adapter and board-facing integration outside this
  repository.

## Source Manifest

`cmake/meshcore_sources.cmake` is the canonical source manifest. Standalone
CMake, Zephyr integration, and tests include the same manifest to avoid private
source-list drift.

## Further Documentation

- `docs/porting.md`: platform hook and runtime API integration guide.
- `docs/testing.md`: independent tests, sync checks, and downstream host
  integration coverage.

## License

The MeshCore C library code in this repository is licensed under the MIT
License. See `LICENSE`.

This library implements MeshCore protocol and runtime behavior using upstream
MeshCore evidence. Upstream MeshCore is licensed under the MIT License; retained
upstream attribution and the MIT notice are in `NOTICE`.

Bundled third-party Monocypher sources keep their upstream
`BSD-2-Clause OR CC0-1.0` license and copyright notices.
