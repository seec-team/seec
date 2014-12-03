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

if ! $program -S -comparable -reverse $1 | cmp -s - $2
then
  $program -S -comparable -reverse $1 | diff - $2
  exit 1
fi
