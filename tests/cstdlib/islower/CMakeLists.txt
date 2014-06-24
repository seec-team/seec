set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}islower-")

add_test(NAME ${SEEC_TEST_PREFIX}ok-printarg-build
         COMMAND ${SEEC_INSTALL}/bin/seec-cc -std=c99 -o ok-printarg ${CMAKE_CURRENT_SOURCE_DIR}/ok-printarg.c)
add_test(NAME ${SEEC_TEST_PREFIX}ok-printarg-run COMMAND ok-printarg)
set_tests_properties(${SEEC_TEST_PREFIX}ok-printarg-run PROPERTIES DEPENDS ${SEEC_TEST_PREFIX}ok-printarg-build)

