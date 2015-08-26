set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}stackrestore-")

seec_test_build(multiple_functions multiple_functions.c "")
seec_test_run_pass(multiple_functions "" "")

