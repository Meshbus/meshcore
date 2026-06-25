# MeshCore Upstream Evidence

This file records the upstream evidence contract used by this MeshCore C
library. `upstream.lock` records the exact reference revision that current sync
checks should verify.

## Locked Reference

| Field | Value |
| --- | --- |
| Reference path | `.reference/meshcore` |
| Commit | `e8d3c53ba1ea863937081cd0caad759b832f3028` |
| Commit summary | `Merge pull request #2753 from formtapez/cli-docs` |
| Upstream license | MIT License; retained in `NOTICE` |

`.reference/meshcore` is read-only evidence. Do not update it as part of a
normal library change. The `.reference/` directory is intentionally ignored by
Git and is not part of release tarballs. When the reference revision changes,
update this file, `upstream.lock`, API maps, and the sync report output in the
same migration phase.

To prepare the reference checkout in a fresh clone:

```sh
mkdir -p .reference
git clone https://github.com/meshcore-dev/MeshCore .reference/meshcore
git -C .reference/meshcore fetch origin e8d3c53ba1ea863937081cd0caad759b832f3028
git -C .reference/meshcore checkout --detach e8d3c53ba1ea863937081cd0caad759b832f3028
```

## Evidence Classification

### Protocol Core

These upstream files are the protocol-core baseline. Their public protocol
classes, constants, fields, packet layouts, and boundary behavior should remain
traceable from C implementation and tests:

- `.reference/meshcore/src/Packet.h`
- `.reference/meshcore/src/Packet.cpp`
- `.reference/meshcore/src/Utils.h`
- `.reference/meshcore/src/Utils.cpp`
- `.reference/meshcore/src/Identity.h`
- `.reference/meshcore/src/Identity.cpp`
- `.reference/meshcore/src/Dispatcher.h`
- `.reference/meshcore/src/Dispatcher.cpp`
- `.reference/meshcore/src/Mesh.h`
- `.reference/meshcore/src/Mesh.cpp`
- `.reference/meshcore/src/MeshCore.h`

### Promoted Support Helpers

These helper files are not protocol core, but their behavior is intentionally
promoted because current generic library behavior depends on their wire format
or generic data structure semantics:

- `.reference/meshcore/src/helpers/StaticPoolPacketManager.h`
- `.reference/meshcore/src/helpers/StaticPoolPacketManager.cpp`
- `.reference/meshcore/src/helpers/SimpleMeshTables.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`
- `.reference/meshcore/src/helpers/TxtDataHelpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.cpp`

Promoted support helpers should be documented as `support` evidence, not as
top-level protocol core. They may still have Layer 1 parity tests when they
define packet-visible data formats.

### Runtime Evidence

These files define observable runtime behavior for the host-driven C runtime:

- `.reference/meshcore/src/helpers/BaseChatMesh.h`
- `.reference/meshcore/src/helpers/BaseChatMesh.cpp`
- `.reference/meshcore/src/helpers/ContactInfo.h`
- `.reference/meshcore/src/helpers/ChannelDetails.h`
- `.reference/meshcore/examples/companion_radio/main.cpp`
- `.reference/meshcore/examples/companion_radio/MyMesh.h`
- `.reference/meshcore/examples/companion_radio/MyMesh.cpp`
- `.reference/meshcore/examples/companion_radio/NodePrefs.h`
- `.reference/meshcore/examples/companion_radio/DataStore.h`
- `.reference/meshcore/examples/companion_radio/DataStore.cpp`

Example files are runtime and adapter evidence only. They do not define
generic protocol-core architecture.

### Host Adapter Or Excluded Evidence

The following upstream areas may be useful background for a host adapter, but
must not be imported into generic library architecture without an
explicit future promotion:

- board classes and RadioLib wrappers;
- serial, BLE, Wi-Fi, ESP-NOW, RS232, and bridge transports;
- display, button, vibration, UI, and CLI helpers;
- concrete file-system, preference, and identity-store implementations;
- sensor manager implementations except for wire payload helpers that are
  explicitly promoted;
- example application UI and persistence flows.

## Sync Rule

When upstream changes, classify each changed file as protocol core, promoted
support, runtime evidence, host adapter evidence, excluded, or deferred before
changing C implementation. The classification should drive source movement,
public API changes, and test placement.

## Sync Check Workflow

Run these checks from the MeshCore repository root before starting an upstream
sync or after changing the MeshCore architecture:

```sh
python3 tools/upstream_lock_check.py --repo-root .
python3 tools/meshcore_sync_report.py --repo-root .
```

The lock check verifies that `.reference/meshcore` is at the commit recorded in
`upstream.lock` and has no local reference-tree changes. It is expected to fail
in a fresh public clone until the reference checkout is prepared with the
commands above. `meshcore_sync_report.py` still runs repository-local boundary
checks without `.reference/meshcore` and reports missing upstream evidence as
warnings.

The sync report groups drift into these categories:

- `upstream lock`: reference checkout does not match `upstream.lock`;
- `protocol core`: locked top-level `.reference/meshcore/src` protocol
  evidence files are missing;
- `promoted support`: helper files intentionally imported as generic support
  evidence are missing;
- `runtime evidence`: helper/example files used for host-driven runtime
  behavior are missing;
- `source manifest`: C implementation files under `src` are
  missing from, duplicated in, or stale in the canonical source manifest;
- `generic include boundary`: Zephyr, Meshbus, Arduino, or host-specific names
  leaked into generic `include/` or `src/` C files;
- `public header ownership`: public headers include private implementation
  headers or non-public dependencies;
- `stale source roots`: build/tool files still reference retired
  `protocol` or `runtime` implementation roots;
- `test hook boundary`: runtime white-box symbols leaked into public headers
  or lost their `MESHCORE_ENABLE_TEST_HOOKS` guard.
