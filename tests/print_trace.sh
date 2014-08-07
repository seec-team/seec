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

$program -R    $* 1>/dev/null
$program -S    $* 1>/dev/null
$program -C -S $* 1>/dev/null

