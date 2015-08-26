set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}errno.h-")

seec_test_build(readwrite-ok readwrite-ok.c "")
seec_test_run_pass_without_comparison(readwrite-ok "" "")

