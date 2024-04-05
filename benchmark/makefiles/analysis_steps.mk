# analyze steps
ANALYSIS_STEPS_SL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,full,tex)
ANALYSIS_STEPS_SL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,partial,tex)

ANALYSIS_STEPS_ALL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,full,tex)
ANALYSIS_STEPS_ALL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,partial,tex)

$(ANALYSIS_DIR)/steps-sl-%.tex: $(BENCHMARK_DIR)/benchmark-sl-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_sl_steps.py $< > $@ || rm $@

$(ANALYSIS_DIR)/steps-all-%.tex: $(BENCHMARK_DIR)/steps-benchmark-all-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_all_steps.py $< > $@ || rm $@

.PHONY: analyze-steps-all-full
analyze-steps-all-full: $(ANALYSIS_STEPS_ALL_FULL)

.PHONY: analyze-steps-sl-full
analyze-steps-sl-full: $(ANALYSIS_STEPS_SL_FULL)

.PHONY: analyze-steps-all-partial
analyze-steps-all-partial: $(ANALYSIS_STEPS_ALL_PARTIAL)

.PHONY: analyze-steps-sl-partial
analyze-steps-sl-partial: $(ANALYSIS_STEPS_SL_PARTIAL)

.PHONY: analyze-steps-all
analyze-steps-all: \
	analyze-steps-all-full \
	analyze-steps-all-partial

.PHONY: analyze-steps-sl
analyze-steps-sl: \
	analyze-steps-sl-full \
	analyze-steps-sl-partial

.PHONY: analyze-steps

analyze-steps: analyze-steps-all analyze-steps-sl
