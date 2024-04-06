.PHONY: analyze-statistics
analyze-statistics: $(ANALYSIS_STATISTICS)

$(ANALYSIS_STATISTICS_DIR)/%.tex: \
		$(STATISTICS_DIR)/%.jsonl | $(ANALYSIS_STATISTICS_DIR) $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_statistics.py $< > $@ || rm $@

$(ANALYSIS_STATISTICS_DIR):
	@mkdir -p $@
