cmake_minimum_required(VERSION 2.8)
project(seec-tests)

set(SEEC_INSTALL "/usr/local" CACHE PATH "Path to SeeC installation.")

set(TEST_ROOT   ${CMAKE_CURRENT_SOURCE_DIR})
set(TEST_SCRIPT ${TEST_ROOT}/run_instrumented.sh)

enable_testing()

macro(seec_test_build BINARY SOURCE ARGS)
 add_test(NAME ${SEEC_TEST_PREFIX}build-${BINARY}
          COMMAND ${SEEC_INSTALL}/bin/seec-cc -std=c99 ${ARGS} -o ${BINARY} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE})
endmacro(seec_test_build)

macro(seec_test_run_pass BINARY TEST ARG)
  add_test(NAME ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}
           COMMAND ${TEST_SCRIPT} SEEC_TRACE_NAME=${BINARY}-${TEST} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY} ${ARG})
  set_tests_properties(${SEEC_TEST_PREFIX}run-${BINARY}-${TEST} PROPERTIES
    DEPENDS ${SEEC_TEST_PREFIX}build-${BINARY})
endmacro(seec_test_run_pass)

macro(seec_test_run_fail BINARY TEST ARG)
  add_test(NAME ${SEEC_TEST_PREFIX}run-${BINARY}-${TEST}
           COMMAND ${TEST_SCRIPT} SEEC_TRACE_NAME=${BINARY}-${TEST} ${CMAKE_CURRENT_BINARY_DIR}/${BINARY} ${ARG})
  set_tests_properties(${SEEC_TEST_PREFIX}run-${BINARY}-${TEST} PROPERTIES
    DEPENDS ${SEEC_TEST_PREFIX}build-${BINARY}
    WILL_FAIL TRUE)
endmacro(seec_test_run_fail)

add_subdirectory(byval)
add_subdirectory(cstdlib)
add_subdirectory(pointers)

