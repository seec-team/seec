set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}bsearch-")

seec_test_build(ok-simple ok-simple.c "")
seec_test_run_pass(ok-simple ""              4)
seec_test_run_fail(ok-simple not-found-low   1)
seec_test_run_fail(ok-simple not-found-mid   5)
seec_test_run_fail(ok-simple not-found-high 14)
seec_test_run_fail(ok-simple err-arg-null   "")

seec_test_build(err-array-null err-array-null.c "")
seec_test_run_fail(err-array-null "" "")

seec_test_build(err-array-overflow err-array-overflow.c "")
seec_test_run_fail(err-array-overflow "" "")

