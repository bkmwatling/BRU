#!/bin/bash
(
python src/compare_sl_dov.py \
  data/benchmark/sl-full-{thompson,flat}-spencer-none.jsonl
echo
python src/compare_sl_dov.py \
  data/benchmark/sl-partial-{thompson,flat}-spencer-none.jsonl
) | sed 's/ & /, /g' | sed 's/ \\\\//'
