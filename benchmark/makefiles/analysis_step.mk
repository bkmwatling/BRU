.PHONY: analyze-step-all
analyze-step-all: $(ANALYSIS_STEP_ALL)

.PHONY: analyze-step-sl
analyze-step-sl: $(ANALYSIS_STEP_SL)


$(ANALYSIS_STEP_DIR)/all-%.tex: \
		$(BENCHMARK_RESULTS_DIR)/all-%.jsonl \
		| $(ANALYSIS_STEP_DIR) $(VENV)
	$(PYTHON) src/analyze_all.py $< "step" > $@ || rm $@

$(ANALYSIS_STEP_DIR)/sl-%.tex: \
		$(BENCHMARK_DIR)/sl-%.jsonl | $(ANALYSIS_STEP_DIR) $(VENV)
	$(PYTHON) src/analyze_sl_step.py $< > $@ || rm $@
