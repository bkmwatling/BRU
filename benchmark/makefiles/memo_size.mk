# MEMO_SIZE
MEMO_SIZE_ALL_FULL := $(join $(dir $(BENCHMARK_ALL_FULL)), $(addprefix memo-size-, $(notdir $(BENCHMARK_ALL_FULL))))
MEMO_SIZE_ALL_PARTIAL := $(join $(dir $(BENCHMARK_ALL_PARTIAL)), $(addprefix memo-size-, $(notdir $(BENCHMARK_ALL_PARTIAL))))
MEMO_SIZE_ALL := $(MEMO_SIZE_ALL_FULL) $(MEMO_SIZE_ALL_PARTIAL)

MEMO_SIZE_SL_FULL := $(join $(dir $(BENCHMARK_SL_FULL)), $(addprefix memo-size-, $(notdir $(BENCHMARK_SL_FULL))))
MEMO_SIZE_SL_PARTIAL := $(join $(dir $(BENCHMARK_SL_PARTIAL)), $(addprefix memo-size-, $(notdir $(BENCHMARK_SL_PARTIAL))))
MEMO_SIZE_SL := $(MEMO_SIZE_SL_FULL) $(MEMO_SIZE_SL_PARTIAL)

.PHONY: append-all-memo_size
append-all-memo-size: $(BENCHMARK_ALL_FULL) $(BENCHMARK_ALL_PARTIAL)
	mkdir -p $(LOGS_DIR)/$(TIMESTAMP)-append-memo-size
	$(PYTHON) src/append_memo_size.py --regex-type all $^ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-append-memo-size/log.log \
		|| rm -f $(MEMO_SIZE_ALL)

.PHONY: append-sl-memo_size
append-sl-memo-size: $(BENCHMARK_SL_FULL) $(BENCHMARK_SL_PARTIAL)
	mkdir -p $(LOGS_DIR)/$(TIMESTAMP)-append-memo-size
	$(PYTHON) src/append_memo_size.py --regex-type sl $^ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-append-memo-size/log.log \
		|| rm -f $(MEMO_SIZE_SL)

$(MEMO_SIZE_SL): append-sl-memo-size
$(MEMO_SIZE_ALL): append-all-memo-size
