.PHONY: statistics
statistics: $(STATISTICS)

.PRECIOUS: $(STATISTICS_DIR)/sl-%.jsonl
.ONESHELL:
$(STATISTICS_DIR)/sl-%.jsonl: $(SL_REGEX_DATASET) | $(STATISTICS_DIR) $(VENV)
	@echo "make $@"
	@args=($(subst -, ,$*))
	@$(PYTHON) src/compilation_statistics.py \
		--construction $${args[0]} --memo-scheme $${args[1]} $< $@ \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log

.PRECIOUS: $(STATISTICS_DIR)/statistics-all-%.jsonl
.ONESHELL:
$(STATISTICS_DIR)/all-%.jsonl: $(ALL_REGEX_DATASET) | $(STATISTICS_DIR) $(VENV)
	@echo "make $@"
	@args=($(subst -, ,$*))
	@$(PYTHON) src/compilation_statistics.py \
		--construction $${args[0]} --memo-scheme $${args[1]} $< $@ \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log
