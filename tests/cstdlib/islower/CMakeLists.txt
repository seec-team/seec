set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}islower-")

seec_test_build(ok-printarg ok-printarg.c)
seec_test_run_pass(ok-printarg "" "")

