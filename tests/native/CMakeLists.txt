# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/meshcore_sources.cmake)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MESHCORE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/../support)

function(meshcore_add_native_test name)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs LABELS SOURCES)
  cmake_parse_arguments(MESHCORE_TEST "${options}" "${oneValueArgs}"
    "${multiValueArgs}" ${ARGN}
  )
  add_executable(${name} ${MESHCORE_TEST_SOURCES})
  target_link_libraries(${name} PRIVATE meshcore::meshcore)
  target_include_directories(${name}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}
      ${MESHCORE_TEST_SUPPORT_DIR}
      ${MESHCORE_INTERNAL_INCLUDE_DIRS}
  )
  add_test(NAME ${name} COMMAND ${name})
  if(MESHCORE_TEST_LABELS)
    set_tests_properties(${name} PROPERTIES LABELS "${MESHCORE_TEST_LABELS}")
  endif()
endfunction()

meshcore_add_native_test(meshcore_native_public_abi
  LABELS boundary api
  SOURCES
  test_public_abi.c
)

meshcore_add_native_test(meshcore_native_packet
  LABELS parity protocol
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_packet_roundtrip.c
)

meshcore_add_native_test(meshcore_native_protocol_parity_vectors
  LABELS parity protocol
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_protocol_parity_vectors.c
)

meshcore_add_native_test(meshcore_native_support_parity
  LABELS parity support
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_support_parity.c
)

meshcore_add_native_test(meshcore_native_runtime_init
  LABELS api runtime
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_runtime_init.c
)

meshcore_add_native_test(meshcore_native_runtime_api_contract
  LABELS api runtime
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_runtime_api_contract.c
)

meshcore_add_native_test(meshcore_native_runtime_pending_oracle
  LABELS runtime api
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_runtime_pending_oracle.c
)

meshcore_add_native_test(meshcore_native_fuzz_smoke
  LABELS fuzz api parity runtime
  SOURCES
  ${MESHCORE_TEST_SUPPORT_DIR}/fake_platform.c
  test_fuzz_smoke.c
)

add_test(NAME meshcore_api_surface_report
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../../tools/api_surface_report.py
    --repo-root ${MESHCORE_LIB_DIR}
    --require-runtime-coverage
    --require-type-coverage
    --require-fake-platform-hooks
)
set_tests_properties(meshcore_api_surface_report PROPERTIES LABELS "api;boundary")

add_test(NAME meshcore_public_header_smoke
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../../tools/public_header_smoke.py
    --repo-root ${MESHCORE_LIB_DIR}
    --work-dir ${CMAKE_CURRENT_BINARY_DIR}/header-smoke
    --cc ${CMAKE_C_COMPILER}
    --cxx ${CMAKE_CXX_COMPILER}
)
set_tests_properties(meshcore_public_header_smoke PROPERTIES LABELS "api;boundary")

add_test(NAME meshcore_parity_coverage_report
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../../tools/parity_coverage_report.py
    --repo-root ${MESHCORE_LIB_DIR}
    --require-full-coverage
)
set_tests_properties(meshcore_parity_coverage_report PROPERTIES LABELS "parity;coverage")

add_test(NAME meshcore_upstream_oracle
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../../tools/upstream_oracle.py
    --repo-root ${MESHCORE_LIB_DIR}
    --meshcore-lib $<TARGET_FILE:meshcore>
    --work-dir ${CMAKE_CURRENT_BINARY_DIR}/upstream-oracle
)
set_tests_properties(meshcore_upstream_oracle PROPERTIES
  LABELS "parity;upstream-oracle"
)

add_test(NAME meshcore_package_smoke
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../../tools/package_smoke.py
    --source-root ${MESHCORE_LIB_DIR}
    --work-dir ${CMAKE_CURRENT_BINARY_DIR}/package-smoke
    --cmake ${CMAKE_COMMAND}
)
set_tests_properties(meshcore_package_smoke PROPERTIES LABELS "package;boundary")
