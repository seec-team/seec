set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}byval-")

seec_test_build(ok-struct ok-struct.c)
seec_test_run_pass(ok-struct "" "")

