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

seec_test_build(ptr_to_static ptr_to_static.c "")
seec_test_run_pass(ptr_to_static "ok-zero"       "0" )
seec_test_run_fail(ptr_to_static "fail-one-past" "10")
seec_test_run_fail(ptr_to_static "fail-low"     "-1" )
seec_test_run_fail(ptr_to_static "fail-high"     "11")

seec_test_build(ptr_to_literal ptr_to_literal.c "")
seec_test_run_pass(ptr_to_literal "ok-zero"       "0" )
seec_test_run_fail(ptr_to_literal "fail-one-past" "10")
seec_test_run_fail(ptr_to_literal "fail-low"     "-1" )
seec_test_run_fail(ptr_to_literal "fail-high"     "11")

