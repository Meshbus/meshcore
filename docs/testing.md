# MeshCore Testing Guide

`meshcore` has an independent native test path and downstream host integration
coverage. Native tests do not require Zephyr, west, or Twister.

## Native CTest

```sh
cmake -S . -B build.meshcore-native \
  -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build build.meshcore-native
ctest --test-dir build.meshcore-native --output-on-failure
```

Native tests use `tests/support/fake_platform.c` to satisfy the public
`meshcore_platform_*` hook contract.

## Package Smoke Test

Install to a temporary prefix and build a downstream project with
`find_package(meshcore CONFIG REQUIRED)`.

```sh
cmake -S . -B build.meshcore-install -DMESHCORE_INSTALL=ON
cmake --build build.meshcore-install
cmake --install build.meshcore-install --prefix /tmp/meshcore-prefix
```

## Sync And Boundary Checks

```sh
python3 tools/meshcore_sync_report.py --repo-root .
```

The sync report checks upstream evidence, source manifest coverage, public
header ownership, stale source roots, test-hook boundaries, platform hook
boundaries, and example public-API usage. If `.reference/meshcore` is not
checked out, upstream evidence checks are reported as warnings while the
repository-local boundary checks still run.

Use `python3 tools/upstream_lock_check.py --repo-root .` only after preparing
the ignored `.reference/meshcore` checkout described in `UPSTREAM.md`; that
standalone lock check intentionally fails when the reference tree is absent.

## Downstream Coverage

Keep platform, board, transport, and product-service integration tests outside
the generic library. Run the relevant downstream host-adapter test suites after
changing public headers, runtime behavior, or source manifest integration.

```sh
# Example shape; replace paths and platforms with the host repository's tests.
west twister -T path/to/host/tests/lib/meshcore -p qemu_x86
west twister -T path/to/host/tests/services/meshcore -p qemu_cortex_m3
west build -d build.host-meshcore-sample \
  -b <board> \
  -s path/to/host/samples/meshcore
```
