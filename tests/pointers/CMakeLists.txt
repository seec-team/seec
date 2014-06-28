set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}pointers-")

seec_test_build(arithmetic arithmetic.c "")
seec_test_run_pass(arithmetic "ok-zero"      "0")
seec_test_run_pass(arithmetic "ok-one-past"  "3")
seec_test_run_fail(arithmetic "fail-low"    "-1")
seec_test_run_fail(arithmetic "fail-high"    "4")

seec_test_build(dereferencing dereferencing.c "")
seec_test_run_pass(dereferencing "ok-zero"       "0")
seec_test_run_fail(dereferencing "fail-one-past" "3")
seec_test_run_fail(dereferencing "fail-low"     "-1")
seec_test_run_fail(dereferencing "fail-high"     "4")

seec_test_build(indexing indexing.c "")
seec_test_run_pass(indexing "ok-zero"       "0")
seec_test_run_fail(indexing "fail-one-past" "3")
seec_test_run_fail(indexing "fail-low"     "-1")
seec_test_run_fail(indexing "fail-high"     "4")

