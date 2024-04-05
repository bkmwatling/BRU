python src/compare_sl_dov.py \
  data/benchmark/benchmark-sl-{thompson,glushkov}-none-spencer-full.jsonl > data/analysis/dov-comparison-full.jsonl

python src/compare_sl_dov.py \
  data/benchmark/benchmark-sl-{thompson,glushkov}-none-spencer-partial.jsonl > data/analysis/dov-comparison-partial.jsonl
