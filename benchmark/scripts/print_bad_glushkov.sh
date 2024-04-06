for match_type in full partial; do
  for memo_scheme_scheduler in spencer-{cn,in,none} lockstep-none; do
    for input_type in positive negative; do
      python src/compare_all_steps.py --input-type "$input_type" \
        data/step/all-"$match_type"-{thompson,flat}-"$memo_scheme_scheduler".jsonl \
        1> case-study/bad_glushkov-"$match_type"-{thompson,flat}-$memo_scheme_scheduler"-"$input_type".txt
    done
  done
done
