.PHONY: benchmark-results-all
benchmark-results-all: $(BENCHMARK_RESULTS_ALL)

.SECONDARY: $(BENCHMARK_RESULTS_ALL)
$(BENCHMARK_RESULTS_ALL): $(BENCHMARK_ALL_FULL) $(BENCHMARK_ALL_PARTIAL) \
		| $(BENCHMARK_RESULTS_DIR) $(LOGS_DIR) $(VENV)
	@echo "make $@"
	@$(PYTHON) src/postprocess_benchmark_results.py --regex-type all \
		$^ $(BENCHMARK_RESULTS_DIR) \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log \
		|| rm -f $(BENCHMARK_RESULTS_ALL)

