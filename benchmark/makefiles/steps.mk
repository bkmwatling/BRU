# STEPS
STEPS_ALL_FULL := $(join $(dir $(BENCHMARK_ALL_FULL)), $(addprefix steps-, $(notdir $(BENCHMARK_ALL_FULL))))
STEPS_ALL_PARTIAL := $(join $(dir $(BENCHMARK_ALL_PARTIAL)), $(addprefix steps-, $(notdir $(BENCHMARK_ALL_PARTIAL))))
STEPS_ALL := $(STEPS_ALL_FULL) $(STEPS_ALL_PARTIAL)

STEPS_SL_FULL := $(join $(dir $(BENCHMARK_SL_FULL)), $(addprefix steps-, $(notdir $(BENCHMARK_SL_FULL))))
STEPS_SL_PARTIAL := $(join $(dir $(BENCHMARK_SL_PARTIAL)), $(addprefix steps-, $(notdir $(BENCHMARK_SL_PARTIAL))))
STEPS_SL := $(STEPS_SL_FULL) $(STEPS_SL_PARTIAL)

.PHONY: append-all-steps
append-all-steps: $(BENCHMARK_ALL_FULL) $(BENCHMARK_ALL_PARTIAL)
	mkdir -p $(LOGS_DIR)/$(TIMESTAMP)-append-steps
	$(PYTHON) src/append_steps.py --regex-type all $^ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-append-steps/log.log \
		|| rm -f $(STEPS_ALL)

.PHONY: append-sl-steps
append-sl-steps: $(BENCHMARK_SL_FULL) $(BENCHMARK_SL_PARTIAL)
	mkdir -p $(LOGS_DIR)/$(TIMESTAMP)-append-steps
	$(PYTHON) src/append_steps.py --regex-type sl $^ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-append-steps/log.log \
		|| rm -f $(STEPS_SL)

$(STEPS_SL): append-sl-steps
$(STEPS_ALL): append-all-steps
