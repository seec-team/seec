set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}qsort-")

seec_test_build(ok-simple ok-simple.c "")
seec_test_run_pass(ok-simple "" "")

