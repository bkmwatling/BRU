VENV := .venv
BRU_PATH := $(shell dirname $(shell pwd))/bin
PYTHON := $(VENV)/bin/python
PIP := $(VENV)/bin/pip
export PATH := $(PATH):$(BRU_PATH)
LOGS_DIR := logs
TIMESTAMP := $(shell date +%Y%m%d-%H%M%S)

DATA_DIR := data
RAW_DATASET_DIR := data-build
INTERMEDIATE_DATASET_DIR := $(RAW_DATASET_DIR)/intermediate
RAW_ALL_REGEX_DATASET := $(RAW_DATASET_DIR)/all_regexes.jsonl
RAW_SL_REGEX_DATASET := $(RAW_DATASET_DIR)/sl_regexes.jsonl
COMPILABLE_ALL_REGEX_DATASET := \
	$(INTERMEDIATE_DATASET_DIR)/compilable_all_regexes.jsonl
COMPILABLE_SL_REGEX_DATASET := \
	$(INTERMEDIATE_DATASET_DIR)/compilable_sl_regexes.jsonl
DATASET_DIR := $(DATA_DIR)/polyglot
ALL_REGEX_DATASET := $(DATASET_DIR)/all_regexes.jsonl
SL_REGEX_DATASET := $(DATASET_DIR)/sl_regexes.jsonl

.DEFAULT:


# Preprocessing

.PHONY: preprocess
preprocess: preprocess-regexes preprocess-sl-regexes

.PHONY: preprocess-regexes
preprocess-regexes: $(ALL_REGEX_DATASET)

.PHONY: preprocess-sl-regexes
preprocess-sl-regexes: $(SL_REGEX_DATASET)

$(COMPILABLE_ALL_REGEX_DATASET): $(RAW_ALL_REGEX_DATASET) | $(VENV) $(DIRS)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/filter_compilable.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log

$(COMPILABLE_SL_REGEX_DATASET): $(RAW_SL_REGEX_DATASET) | $(VENV) $(DIRS)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/filter_compilable.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log

$(ALL_REGEX_DATASET): $(COMPILABLE_ALL_REGEX_DATASET) | $(VENV) $(DIRS)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/preprocess_all_regexes.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log

$(SL_REGEX_DATASET): $(COMPILABLE_SL_REGEX_DATASET) | $(VENV) $(DIRS)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/preprocess_sl_regexes.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log


REGEX_TYPES := all sl
CONSTRUCTIONS := thompson glushkov
MEMO_SCHEMES := none cn in
SCHEDULERS := spencer lockstep

define target_variables_template
$(foreach construction,$(CONSTRUCTIONS),\
	$(foreach memo_scheme,$(MEMO_SCHEMES),\
		$(1)/$(2)-$(3)-$(construction)-$(memo_scheme)-spencer.$(4)\
	)\
) \
$(foreach construction,$(CONSTRUCTIONS),\
	$(1)/$(2)-$(3)-$(construction)-none-lockstep.$(4)\
)
endef

define eval_template
$(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(foreach memo_scheme,$(MEMO_SCHEMES),\
			$(eval $(call $(1),$(regex_type),$(construction),$(memo_scheme),spencer))\
		)\
	)\
) \
$(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(eval $(call $(1),$(regex_type),$(construction),none,lockstep))\
	)\
)
endef


# Benchmarking
BENCHMARK_DIR := $(DATA_DIR)/benchmark

define benchmark_template
$(BENCHMARK_DIR)/benchmark-$(1)-$(2)-$(3)-$(4).jsonl: $(REGEX_DATASET) | $(BENCHMARK_DIR) $(VENV)
	make $(LOGS_DIR)/$(TIMESTAMP)-$$(basename $$(notdir $$@))
	$(PYTHON) src/benchmark.py \
		--regex-type $(1) \
		--construction $(2) \
		--memo-scheme $(3) \
		--scheduler $(4) \
		$(DATASET_DIR)/$(1)_regexes.jsonl $$@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$$(basename $$(notdir $$@))/log.log
endef

$(call eval_template,benchmark_template)  # generate benchmark target recipes

BENCHMARK_SL := $(call target_variables_template,$(BENCHMARK_DIR),benchmark,sl,json)
BENCHMARK_ALL := $(call target_variables_template,$(BENCHMARK_DIR),benchmark,all,json)

.PHONY: benchmark
benchmark: benchmark-all benchmark-sl

.PHONY: benchmark-sl
benchmark-sl: $(BENCHMARK_SL)

.PHONY: benchmark-all
benchmark-all: $(BENCHMARK_ALL)


# Analysis
ANALYSIS_DIR := $(DATA_DIR)/analysis

STEPS_SL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,tex)
STEPS_ALL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,tex)


$(ANALYSIS_DIR)/steps-%.tex: $(BENCHMARK_DIR)/benchmark-%.jsonl | $(ANALYSIS_DIR) $(VENV)
	$(PYTHON) src/analysis_steps.py \
		--regex-type $(word 1,$(subst -, ,$*)) $< > $@ || rm $@

analysis-steps-all: $(STEPS_ALL)

analysis-steps-sl: $(STEPS_SL)

.PHONY: analysis-steps
analysis-steps: analysis-steps-all analysis-steps-sl

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
	$(ANALYSIS_DIR)

$(DIRS):
	mkdir -p $@


.PHONY: clean
clean:
	rm -rf $(VENV)
	rm -rf $(LOGS_DIR)

.PHONY: vim
vim:
	vim $$(git ls-files) $$(git ls-files --others --exclude-standard)
