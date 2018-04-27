#!/bin/bash

SEEC_PRINT="${SEEC_PRINT:-seec-print}"
TESTS_RUN_DIR=${1}
TESTS_SRC_DIR=${2}

for file in $( find ${TESTS_RUN_DIR} | grep ".seec" )
do
  if [ -e "${TESTS_SRC_DIR}/${file%.seec}.expected" ]
  then
    $SEEC_PRINT -S -comparable -reverse $file > "${TESTS_SRC_DIR}/${file%.seec}.expected"
  fi
done

