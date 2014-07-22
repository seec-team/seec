set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}strtok-")

seec_test_build(printarg printarg.c "")
seec_test_run_pass(printarg "ok" "one:two:three")

