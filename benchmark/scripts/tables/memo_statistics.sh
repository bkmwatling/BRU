#!bash
(
  for file in data/analysis/memo_size/all-{full,partial}-{thompson,flat}-spencer-{cn,in}.tex; do
    sed -n -e '3p' -e '6p' $file | tr -d '\n' | sed 's/\\\\/\&/'
    echo
  done
) | sed 's/ & /, /g' | sed 's/ \\\\//'
