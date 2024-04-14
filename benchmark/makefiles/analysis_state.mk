.PHONY: analyze-state-all
analyze-state-all: $(ANALYSIS_STATE_ALL)

$(ANALYSIS_STATE_DIR)/all-%.tex: \
		$(BENCHMARK_RESULTS_DIR)/all-%.jsonl \
		| $(ANALYSIS_STATE_DIR) $(VENV)
	$(PYTHON) src/analyze_all.py $< "state" > $@ || rm $@
