# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0

# Canonical source manifest for the platform-neutral MeshCore C library.
#
# Include this file from Zephyr tests, Meshbus adapters, or future standalone
# builds instead of maintaining a private complete source list.

get_filename_component(MESHCORE_LIB_DIR
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE
)

set(MESHCORE_PUBLIC_INCLUDE_DIRS
  ${MESHCORE_LIB_DIR}/include
)

set(MESHCORE_INTERNAL_INCLUDE_DIRS
  ${MESHCORE_LIB_DIR}/include
  ${MESHCORE_LIB_DIR}/src/core
  ${MESHCORE_LIB_DIR}/src/support
  ${MESHCORE_LIB_DIR}/src/support/crypto
  ${MESHCORE_LIB_DIR}/src/support/telemetry
  ${MESHCORE_LIB_DIR}/src/runtime
  ${MESHCORE_LIB_DIR}/src/platform
)

set(MESHCORE_PROTOCOL_PACKET_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_packet.c
)

set(MESHCORE_PROTOCOL_UTILS_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_utils.c
)

set(MESHCORE_PROTOCOL_RNG_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_rng.c
)

set(MESHCORE_PROTOCOL_CLOCK_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_clock.c
)

set(MESHCORE_PROTOCOL_RADIO_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_radio.c
)

set(MESHCORE_PROTOCOL_PACKET_MANAGER_SOURCES
  ${MESHCORE_LIB_DIR}/src/support/meshcore_packet_manager.c
)

set(MESHCORE_PROTOCOL_DISPATCHER_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_dispatcher.c
)

set(MESHCORE_PROTOCOL_IDENTITY_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_identity.c
)

set(MESHCORE_PROTOCOL_TABLES_SOURCES
  ${MESHCORE_LIB_DIR}/src/support/meshcore_tables.c
)

set(MESHCORE_PROTOCOL_MESH_SOURCES
  ${MESHCORE_LIB_DIR}/src/core/meshcore_mesh.c
)

set(MESHCORE_SUPPORT_ADVERT_DATA_SOURCES
  ${MESHCORE_LIB_DIR}/src/support/meshcore_advert_data.c
)

set(MESHCORE_SUPPORT_CAYENNE_LPP_SOURCES
  ${MESHCORE_LIB_DIR}/src/support/telemetry/meshcore_cayenne_lpp_compat.c
)

set(MESHCORE_PROTOCOL_CRYPTO_SOURCES
  ${MESHCORE_LIB_DIR}/src/support/crypto/monocypher.c
  ${MESHCORE_LIB_DIR}/src/support/crypto/monocypher-ed25519.c
)

set(MESHCORE_PLATFORM_BRIDGE_SOURCES
  ${MESHCORE_LIB_DIR}/src/platform/meshcore_platform_bridge.c
)

set(MESHCORE_RUNTIME_SOURCES
  ${MESHCORE_LIB_DIR}/src/runtime/meshcore.c
  ${MESHCORE_LIB_DIR}/src/runtime/meshcore_runtime_pending.c
  ${MESHCORE_PLATFORM_BRIDGE_SOURCES}
  ${MESHCORE_LIB_DIR}/src/runtime/meshcore_runtime_request.c
)

set(MESHCORE_PROTOCOL_CORE_SOURCES
  ${MESHCORE_PROTOCOL_PACKET_SOURCES}
  ${MESHCORE_PROTOCOL_UTILS_SOURCES}
  ${MESHCORE_PROTOCOL_RNG_SOURCES}
  ${MESHCORE_PROTOCOL_CLOCK_SOURCES}
  ${MESHCORE_PROTOCOL_RADIO_SOURCES}
  ${MESHCORE_PROTOCOL_PACKET_MANAGER_SOURCES}
  ${MESHCORE_PROTOCOL_DISPATCHER_SOURCES}
  ${MESHCORE_PROTOCOL_IDENTITY_SOURCES}
  ${MESHCORE_PROTOCOL_TABLES_SOURCES}
  ${MESHCORE_PROTOCOL_MESH_SOURCES}
)

set(MESHCORE_SUPPORT_SOURCES
  ${MESHCORE_SUPPORT_ADVERT_DATA_SOURCES}
  ${MESHCORE_SUPPORT_CAYENNE_LPP_SOURCES}
)

set(MESHCORE_RUNTIME_LIBRARY_SOURCES
  ${MESHCORE_RUNTIME_SOURCES}
  ${MESHCORE_SUPPORT_SOURCES}
  ${MESHCORE_PROTOCOL_CORE_SOURCES}
  ${MESHCORE_PROTOCOL_CRYPTO_SOURCES}
)

set(MESHCORE_ALL_LIBRARY_SOURCES
  ${MESHCORE_RUNTIME_LIBRARY_SOURCES}
)
