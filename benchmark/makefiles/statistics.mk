.SECONDARY: $(STATISTICS_DIR)/sl-%.jsonl
$(STATISTICS_DIR)/sl-%.jsonl: \
		$(SL_REGEX_DATASET) \
		| $(STATISTICS_DIR) $(VENV) $(LOGS_DIR)/statistics
	@echo "make $@"
	@args=($(subst -, ,$*))
	@$(PYTHON) src/compilation_statistics.py \
		--construction $${args[0]} --memo-scheme $${args[1]} $< $@ \
		2> $(LOGS_DIR)/statistics/$(basename $(notdir $@)).log

.SECONDARY: $(STATISTICS_DIR)/statistics-all-%.jsonl
$(STATISTICS_DIR)/all-%.jsonl: \
		$(ALL_REGEX_DATASET) \
		| $(STATISTICS_DIR) $(VENV) $(LOGS_DIR)/statistics
	@echo "make $@"
	@args=($(subst -, ,$*))
	@$(PYTHON) src/compilation_statistics.py \
		--construction $${args[0]} --memo-scheme $${args[1]} $< $@ \
		2> $(LOGS_DIR)/statistics/$(basename $(notdir $@)).log
