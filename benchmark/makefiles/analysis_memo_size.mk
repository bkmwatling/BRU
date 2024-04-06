.PHONY: analyze-memo-size
analyze-memo-size: analyze-memo-size-all

.PHONY: analyze-memo-size-all
analyze-memo-size-all: $(ANALYSIS_MEMO_SIZE_ALL)

.PHONY: analyze-memo-size-sl
analyze-memo-size-sl: $(ANALYSIS_MEMO_SIZE_SL)


$(ANALYSIS_MEMO_SIZE_DIR)/all-%.tex: \
		$(MEMO_SIZE_DIR)/all-%.jsonl | $(ANALYSIS_MEMO_SIZE_DIR) $(VENV)
	$(PYTHON) src/analyze_all_memo_size.py $< > $@ || rm $@


$(ANALYSIS_MEMO_SIZE_DIR)/sl-%.tex: \
		$(MEMO_SIZE_DIR)/sl-%.jsonl | $(ANALYSIS_MEMO_SIZE_DIR) $(VENV)
	$(PYTHON) src/analyze_sl_memo_size.py $< > $@ || rm $@

$(ANALYSIS_MEMO_SIZE_DIR):
	@mkdir -p $@
