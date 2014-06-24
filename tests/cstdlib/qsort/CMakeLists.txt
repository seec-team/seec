set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}qsort-")

add_test(NAME ${SEEC_TEST_PREFIX}ok-simple-build
         COMMAND ${SEEC_INSTALL}/bin/seec-cc -std=c99 -o ok-simple ${CMAKE_CURRENT_SOURCE_DIR}/ok-simple.c)
add_test(NAME ${SEEC_TEST_PREFIX}ok-simple-run COMMAND ok-simple)
set_tests_properties(${SEEC_TEST_PREFIX}ok-simple-run PROPERTIES DEPENDS ${SEEC_TEST_PREFIX}ok-simple-build)

