set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}longdouble-")

seec_test_build(print print.c "")
seec_test_run_pass(print "" "")

seec_test_build(pass_return pass_return.c "")
seec_test_run_pass(pass_return "" "")

