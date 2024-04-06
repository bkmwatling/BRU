# MEMO_SIZE

.PHONY: append-memo-size
append-memo-size: append-all-memo-size

.PHONY: append-all-memo-size
append-all-memo-size: $(MEMO_SIZE_ALL)

.PHONY: append-sl-memo-size
append-sl-memo-size: $(MEMO_SIZE_SL)

$(MEMO_SIZE_ALL): $(BENCHMARK_ALL_FULL) $(BENCHMARK_ALL_PARTIAL) \
		| $(MEMO_SIZE_DIR) $(LOGS_DIR) $(VENV)
	@echo "make $@"
	@$(PYTHON) src/append_memo_size.py --regex-type all \
		$^ $(MEMO_SIZE_DIR) \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log \
		|| rm -f $(MEMO_SIZE_ALL)

$(MEMO_SIZE_SL): $(BENCHMARK_SL_FULL) $(BENCHMARK_SL_PARTIAL) \
		| $(MEMO_SIZE_DIR) $(LOGS_DIR) $(VENV)
	@echo "make $@"
	@$(PYTHON) src/append_memo_size.py --regex-type sl \
		$^ $(MEMO_SIZE_DIR) \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log \
		|| rm -f $(MEMO_SIZE_SL)
