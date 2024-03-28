for match_type in full partial; do
  for memo_scheme_scheduler in cn-spencer in-spencer none-spencer none-lockstep; do
    for input_type in positive negative; do
      python src/compare_all_steps.py --input-type "$input_type" \
        data/benchmark/steps-benchmark-all-{thompson,glushkov}-"$memo_scheme_scheduler"-"$match_type".jsonl \
        1> case-study/bad_glushkov-"$memo_scheme_scheduler"-"$match_type"-"$input_type".txt
    done
  done
done
