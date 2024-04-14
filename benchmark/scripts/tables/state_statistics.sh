#!/bin/bash
sed -n -e '1,2p' -e '4,5p' data/analysis/state/all-full-thompson-spencer-none.tex
(
  for file in data/analysis/state/all-{full,partial}-{thompson,flat}-{spencer-{none,cn,in},lockstep-none}.tex; do
    sed -n -e '3p' -e '6p' $file | tr -d '\n' | sed 's/\\\\/\&/'
    echo
  done
) | sed 's/ & /, /g' | sed 's/ \\\\//'
