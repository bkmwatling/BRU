.PHONY: analyze-step
analyze-step: analyze-step-all analyze-step-sl

.PHONY: analyze-step-all
analyze-step-all: $(ANALYSIS_STEP_ALL)

.PHONY: analyze-step-sl
analyze-step-sl: $(ANALYSIS_STEP_SL)


$(ANALYSIS_STEP_DIR)/all-%.tex: \
		$(STEP_DIR)/all-%.jsonl | $(ANALYSIS_STEP_DIR) $(VENV)
	$(PYTHON) src/analyze_all_step.py $< > $@ || rm $@

$(ANALYSIS_STEP_DIR)/sl-%.tex: \
		$(BENCHMARK_DIR)/sl-%.jsonl | $(ANALYSIS_STEP_DIR) $(VENV)
	$(PYTHON) src/analyze_sl_step.py $< > $@ || rm $@

$(ANALYSIS_STEP_DIR):
	@mkdir -p $@
