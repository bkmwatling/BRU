.PHONY: analyze-memo-size-all
analyze-memo-size-all: $(ANALYSIS_MEMO_SIZE_ALL)

$(ANALYSIS_MEMO_SIZE_DIR)/all-%.tex: \
		$(BENCHMARK_RESULTS_DIR)/all-%.jsonl \
		| $(ANALYSIS_MEMO_SIZE_DIR) $(VENV)
	$(PYTHON) src/analyze_all.py $< "memo_entry" > $@ || rm $@
