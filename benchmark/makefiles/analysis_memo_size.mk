# analyze memo size
ANALYSIS_MEMO_SIZE_SL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),memo-size,sl,full,tex)
ANALYSIS_MEMO_SIZE_SL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),memo-size,sl,partial,tex)

ANALYSIS_MEMO_SIZE_ALL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),memo-size,all,full,tex)
ANALYSIS_MEMO_SIZE_ALL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),memo-size,all,partial,tex)

$(ANALYSIS_DIR)/memo-size-sl-%.tex: $(BENCHMARK_DIR)/benchmark-sl-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_sl_memo_size.py $< > $@ || rm $@

$(ANALYSIS_DIR)/memo-size-all-%.tex: $(BENCHMARK_DIR)/memo_size-benchmark-all-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_all_memo_size.py $< > $@ || rm $@

.PHONY: analyze-memo-size-all
analyze-memo-size-all: \
	analyze-memo-size-all-full \
	analyze-memo-size-all-partial

.PHONY: analyze-memo-size-sl
analyze-memo-size-sl: \
	analyze-memo-size-sl-full \
	analyze-memo-size-sl-partial

.PHONY: analyze-memo-size-all-full
analyze-memo-size-all-full: $(ANALYSIS_MEMO_SIZE_ALL_FULL)

.PHONY: analyze-memo-size-sl-full
analyze-memo-size-sl-full: $(ANALYSIS_MEMO_SIZE_SL_FULL)

.PHONY: analyze-memo-size-all-partial
analyze-memo-size-all-partial: $(ANALYSIS_MEMO_SIZE_ALL_PARTIAL)

.PHONY: analyze-memo-size-sl-partial
analyze-memo-size-sl-partial: $(ANALYSIS_MEMO_SIZE_SL_PARTIAL)
