set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}realloc-")

seec_test_build(correct correct.c "")
seec_test_run_pass(correct "" "")

