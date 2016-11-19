set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}unistd.h-")

seec_test_build(sleep sleep.c "")
seec_test_run_pass(sleep "" "")

