set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}byval-")

add_test(NAME ${SEEC_TEST_PREFIX}ok-struct-build
         COMMAND ${SEEC_INSTALL}/bin/seec-cc -std=c99 -o ok-struct ${CMAKE_CURRENT_SOURCE_DIR}/ok-struct.c)
add_test(NAME ${SEEC_TEST_PREFIX}ok-struct-run COMMAND ok-struct)
set_tests_properties(${SEEC_TEST_PREFIX}ok-struct-run PROPERTIES DEPENDS ${SEEC_TEST_PREFIX}ok-struct-build)

