# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/meshcore_sources.cmake)

set(MESHCORE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/../support)

function(meshcore_add_native_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE meshcore::meshcore)
  target_include_directories(${name}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}
      ${MESHCORE_TEST_SUPPORT_DIR}
      ${MESHCORE_INTERNAL_INCLUDE_DIRS}
  )
  add_test(NAME ${name} COMMAND ${name})
endfunction()

meshcore_add_native_test(meshcore_native_public_abi
  test_public_abi.c
)

meshcore_add_native_test(meshcore_native_packet
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_packet_roundtrip.c
)

meshcore_add_native_test(meshcore_native_runtime_init
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_runtime_init.c
)
