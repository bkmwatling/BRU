# STEP

.PHONY: append-step
append-step: append-all-step

.PHONY: append-all-step
append-all-step: $(STEP_ALL)

.PRECIOUS: $(STEP_ALL)
$(STEP_ALL): $(BENCHMARK_ALL_FULL) $(BENCHMARK_ALL_PARTIAL) \
		| $(STEP_DIR) $(LOGS_DIR) $(VENV)
	@echo "make $@"
	@$(PYTHON) src/append_step.py --regex-type all $^ $(STEP_DIR)\
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log \
		|| rm -f $(STEP_ALL)
