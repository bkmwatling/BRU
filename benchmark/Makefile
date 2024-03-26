VENV := .venv
BRU_PATH := $(shell dirname $(shell pwd))/bin
PYTHON := $(VENV)/bin/python
PIP := $(VENV)/bin/pip
export PATH := $(PATH):$(BRU_PATH)
LOGS_DIR := logs
TIMESTAMP := $(shell date +%Y%m%d-%H%M%S)

DATA_DIR := data
RAW_DATASET_DIR := data-build
RAW_ALL_REGEX_DATASET := $(RAW_DATASET_DIR)/all_regexes.jsonl
RAW_SL_REGEX_DATASET := $(RAW_DATASET_DIR)/sl_regexes.jsonl

DATASET_DIR := $(DATA_DIR)/polyglot
ALL_REGEX_DATASET := $(DATASET_DIR)/all_regexes.jsonl
SL_REGEX_DATASET := $(DATASET_DIR)/sl_regexes.jsonl

.DEFAULT:

REGEX_TYPES := all sl
CONSTRUCTIONS := thompson glushkov
MEMO_SCHEMES := none cn in
SCHEDULERS := spencer lockstep
MATCHING_TYPES := full partial

include makefiles/preprocessing.mk

# $(1) = directory, $(2) = prefix, $(3) = regex_type, $(4) = matching_type, $(5) = extension
define target_variables_template
$(foreach construction,$(CONSTRUCTIONS),\
	$(foreach memo_scheme,$(MEMO_SCHEMES),\
		$(1)/$(2)-$(3)-$(construction)-$(memo_scheme)-spencer-$(4).$(5)\
	)\
) \
$(foreach construction,$(CONSTRUCTIONS),\
	$(1)/$(2)-$(3)-$(construction)-none-lockstep-$(4).$(5)\
)
endef

# $(1) = template which must take 5 arguments:
# regex_type, construction, memo_scheme, scheduler, matching_type
define eval_template
$(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(foreach memo_scheme,$(MEMO_SCHEMES),\
			$(foreach matching_type,$(MATCHING_TYPES),\
				$(eval $(call $(1),$(regex_type),$(construction),$(memo_scheme),spencer,$(matching_type)))\
			)\
		)\
	)\
) \
$(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(foreach matching_type,$(MATCHING_TYPES),\
			$(eval $(call $(1),$(regex_type),$(construction),none,lockstep,$(matching_type)))\
		)\
	)\
)
endef

include makefiles/benchmark.mk
include makefiles/steps.mk
include makefiles/statistics.mk

# Analysis
ANALYSIS_DIR := $(DATA_DIR)/analysis

# analyze steps
ANALYSIS_STEPS_SL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,full,tex)
ANALYSIS_STEPS_SL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,partial,tex)

ANALYSIS_STEPS_ALL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,full,tex)
ANALYSIS_STEPS_ALL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,partial,tex)

$(ANALYSIS_DIR)/steps-sl-%.tex: $(BENCHMARK_DIR)/benchmark-sl-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_sl_steps.py $< > $@ || rm $@

$(ANALYSIS_DIR)/steps-all-%.tex: $(BENCHMARK_DIR)/steps-benchmark-all-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analyze_all_steps.py $< > $@ || rm $@

.PHONY: analyze-steps-all
analyze-steps-all: \
	analyze-steps-all-full \
	analyze-steps-all-partial

.PHONY: analyze-steps-sl
analyze-steps-sl: \
	analyze-steps-sl-full \
	analyze-steps-sl-partial

.PHONY: analyze-steps-all-full
analyze-steps-all-full: $(ANALYSIS_STEPS_ALL_FULL)

.PHONY: analyze-steps-sl-full
analyze-steps-sl-full: $(ANALYSIS_STEPS_SL_FULL)

.PHONY: analyze-steps-all-partial
analyze-steps-all-partial: $(ANALYSIS_STEPS_ALL_PARTIAL)

.PHONY: analyze-steps-sl-partial
analyze-steps-sl-partial: $(ANALYSIS_STEPS_SL_PARTIAL)

# Analyze Statistics
ANALYSIS_STATS_SL := $(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(foreach memo_scheme,$(MEMO_SCHEMES),\
			$(ANALYSIS_DIR)/statistics-$(regex_type)-$(construction)-$(memo_scheme).tex\
		)\
	)\
)

$(ANALYSIS_DIR)/statistics-%.tex: $(STATISTICS_DIR)/statistics-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analysis_statistics.py $< > $@ || rm $@

.PHONY: analysis-statistics
analysis-statistics: $(ANALYSIS_STATS_SL)

# Misc
$(LOGS_DIR)/%: | $(LOGS_DIR)
	@mkdir -p $@

$(VENV): requirements.txt
	python3 -m venv .venv
	touch $(VENV)
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements.txt


DIRS := \
	$(DATA_DIR) \
	$(DATASET_DIR) \
	$(RAW_DATASET_DIR) \
	$(LOGS_DIR) \
	$(INTERMEDIATE_DATASET_DIR) \
	$(BENCHMARK_DIR) \
	$(ANALYSIS_DIR) \
	$(STATISTICS_DIR)

$(DIRS):
	mkdir -p $@

%.tar.gz: %
	tar -czvf $@ $<

%.tar.xz: %
	tar -cJvf $@ $<

.PHONY: clean
clean:
	rm -rf $(VENV)
	rm -rf $(LOGS_DIR)

.PHONY: vim
vim:
	vim $$(git ls-files | grep -v '.*\.tex') $$(git ls-files --others --exclude-standard)
