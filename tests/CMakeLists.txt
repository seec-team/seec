cmake_minimum_required(VERSION 2.8)
project(seec-tests)

set(SEEC_INSTALL "/usr/local" CACHE PATH "Path to SeeC installation.")

set(TEST_ROOT   ${CMAKE_CURRENT_SOURCE_DIR})
set(TEST_SCRIPT ${TEST_ROOT}/run_instrumented.sh)
set(TEST_PRINT  ${TEST_ROOT}/print_trace.sh)
set(TEST_PRINT_COMPARE ${TEST_ROOT}/print_compare_trace.sh)

enable_testing()

set(SEEC_CC_FLAGS "")
if(NOT "${CMAKE_OSX_SYSROOT}" STREQUAL "")
  set(SEEC_CC_FLAGS "${SEEC_CC_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT}")
endif(NOT "${CMAKE_OSX_SYSROOT}" STREQUAL "")

macro(seec_test_build BINARY SOURCE ARGS)
 add_test(NAME ${SEEC_TEST_PREFIX}build-${BINARY}
          COMMAND ${TEST_SCRIPT} SEEC_WRITE_INSTRUMENTED=${BINARY}.instrumented.ll ${SEEC_INSTALL}/bin/seec-cc ${SEEC_CC_FLAGS} -std=c99 -fvisibility=hidden ${ARGS} -o ${BINARY} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE})
endmacro(seec_test_build)

macro(seec_test_print_trace BINARY TEST)
  add_test(NAME ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}-print-trace
           COMMAND ${TEST_PRINT} ${SEEC_INSTALL}/bin/seec-print ${BINARY}-${TEST}.seec)
  set_tests_properties(${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}-print-trace PROPERTIES
    DEPENDS ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST})
endmacro(seec_test_print_trace)

macro(seec_test_print_trace_compare BINARY TEST)
  add_test(NAME ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}-compare-trace
           COMMAND ${TEST_PRINT_COMPARE} ${SEEC_INSTALL}/bin/seec-print ${BINARY}-${TEST}.seec ${CMAKE_CURRENT_SOURCE_DIR}/${BINARY}-${TEST}.expected)
  set_tests_properties(${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}-compare-trace PROPERTIES
    DEPENDS ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST})
endmacro(seec_test_print_trace_compare)

macro(seec_test_run_pass_without_comparison BINARY TEST ARG)
  add_test(NAME ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}
           COMMAND ${TEST_SCRIPT} SEEC_TRACE_NAME=${BINARY}-${TEST} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY} ${ARG})
  set_tests_properties(${SEEC_TEST_PREFIX}run-${BINARY}-${TEST} PROPERTIES
    DEPENDS ${SEEC_TEST_PREFIX}build-${BINARY})
  seec_test_print_trace(${BINARY} "${TEST}")
endmacro(seec_test_run_pass_without_comparison)

macro(seec_test_run_pass BINARY TEST ARG)
  seec_test_run_pass_without_comparison(${BINARY} "${TEST}" "${ARG}")
  seec_test_print_trace_compare(${BINARY} "${TEST}")
endmacro(seec_test_run_pass)

macro(seec_test_run_fail_without_comparison BINARY TEST ARG)
  add_test(NAME ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}
           COMMAND ${TEST_SCRIPT} SEEC_TRACE_NAME=${BINARY}-${TEST} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY} ${ARG})
  set_tests_properties(${SEEC_TEST_PREFIX}run-${BINARY}-${TEST} PROPERTIES
    DEPENDS ${SEEC_TEST_PREFIX}build-${BINARY}
    WILL_FAIL TRUE)
  seec_test_print_trace(${BINARY} "${TEST}")
endmacro(seec_test_run_fail_without_comparison)

macro(seec_test_run_fail BINARY TEST ARG)
  seec_test_run_fail_without_comparison(${BINARY} "${TEST}" "${ARG}")
  seec_test_print_trace_compare(${BINARY} "${TEST}")
endmacro(seec_test_run_fail)

add_subdirectory(byval)
add_subdirectory(cstdlib)
add_subdirectory(longdouble)
add_subdirectory(pointers)
add_subdirectory(stackrestore)
add_subdirectory(streams)

