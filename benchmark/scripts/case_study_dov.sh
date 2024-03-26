files=(data/benchmark/benchmark-sl-*-{cn,in}-spencer-full.jsonl)
files+=(data/benchmark/benchmark-sl-*-none-lockstep-full.jsonl)
for file in "${files[@]}"; do
  python src/analyze_sl_steps.py --info "$file" 2> case-study/"$(basename -s .jsonl "${file}")".txt
done
