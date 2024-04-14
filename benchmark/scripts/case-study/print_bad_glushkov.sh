#!/bin/bash
PREFIX=case-study/bad-glushkov
mkdir -p "${PREFIX}"
for match_type in full partial; do
  for memo_scheme_scheduler in spencer-{cn,in,none} lockstep-none; do
    for input_type in positive negative; do
      python src/compare_all_steps.py \
        --input-type ${input_type} \
        data/benchmark_results/all-${match_type}-{thompson,flat}-${memo_scheme_scheduler}.jsonl \
        1> "${PREFIX}/${match_type}-${memo_scheme_scheduler}-${input_type}.txt"
    done
  done
done

PREFIX=case-study/bad-glushkov/threshold_10
mkdir -p "${PREFIX}"
for match_type in full partial; do
  for memo_scheme_scheduler in spencer-{cn,in,none} lockstep-none; do
    for input_type in positive negative; do
      python src/compare_all_steps.py \
        --input-type ${input_type} \
        --threshold 10 \
        data/benchmark_results/all-${match_type}-{thompson,flat}-${memo_scheme_scheduler}.jsonl \
        1> "${PREFIX}/${match_type}-${memo_scheme_scheduler}-${input_type}.txt"
    done
  done
done
