BENCHMARK_DIR := $(DATA_DIR)/benchmark

define benchmark_template
$(BENCHMARK_DIR)/benchmark-$(1)-$(2)-$(3)-$(4)-$(5).jsonl: $(DATASET_DIR)/$(1)_regexes.jsonl | $(BENCHMARK_DIR) $(VENV)
	make $(LOGS_DIR)/$(TIMESTAMP)-$$(basename $$(notdir $$@))
	$(PYTHON) src/benchmark.py \
		--regex-type $(1) \
		--construction $(2) \
		--memo-scheme $(3) \
		--scheduler $(4) \
		--matching_type $(5) \
		$(DATASET_DIR)/$(1)_regexes.jsonl $$@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$$(basename $$(notdir $$@))/log.log
endef

$(call eval_template,benchmark_template)  # generate benchmark target recipes

BENCHMARK_SL_FULL := $(call target_variables_template,$(BENCHMARK_DIR),benchmark,sl,full,jsonl)
BENCHMARK_ALL_FULL := $(call target_variables_template,$(BENCHMARK_DIR),benchmark,all,full,jsonl)
BENCHMARK_SL_PARTIAL := $(call target_variables_template,$(BENCHMARK_DIR),benchmark,sl,partial,jsonl)
BENCHMARK_ALL_PARTIAL := $(call target_variables_template,$(BENCHMARK_DIR),benchmark,all,partial,jsonl)

.PHONY: benchmark
benchmark: benchmark-sl-full benchmark-all-full

.PHONY: benchmark-sl
benchmark-sl: benchmark-sl-full benchmark-sl-partial

.PHONY: benchmark-all
benchmark-all: benchmark-all-full benchmark-all-partial

.PHONY: benchmark-sl-full
benchmark-sl-full: $(BENCHMARK_SL_FULL)

.PHONY: benchmark-all-full
benchmark-all-full: $(BENCHMARK_ALL_FULL)

.PHONY: benchmark-sl-partial
benchmark-sl-partial: $(BENCHMARK_SL_PARTIAL)

.PHONY: benchmark-all-partial
benchmark-all-partial: $(BENCHMARK_ALL_PARTIAL)
