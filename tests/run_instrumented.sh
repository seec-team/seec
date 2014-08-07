#!/bin/sh

until [ -z "$1" ]
do
  if echo "$1" | grep -q "="
  then
    variable=${1%%=*} # extract name
    value=${1##*=}    # extract value
    export $variable=$value
    shift
  else
    break
  fi
done

program=$1
shift

if [ -z "$SEEC_TEST_SILENCE_STDOUT" ]
then
  $program $* 1>/dev/null
else
  $program $*
fi

