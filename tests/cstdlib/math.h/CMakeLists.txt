set(SEEC_TEST_PREFIX "${SEEC_TEST_PREFIX}math.h-")

seec_test_build(frexp-ok frexp-ok.c -lm)
seec_test_run_pass(frexp-ok "" "")

seec_test_build(modf-ok modf-ok.c -lm)
seec_test_run_pass(modf-ok "" "")

seec_test_build(nan-ok nan-ok.c -lm)
seec_test_run_pass(nan-ok "" "")

seec_test_build(remquo-ok remquo-ok.c -lm)
seec_test_run_pass(remquo-ok "" "")

