set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}streams-")

seec_test_build(print_n print_n.c "")
seec_test_run_pass(print_n "zero"  "0 hello")
seec_test_run_pass(print_n "three" "3 hello")

