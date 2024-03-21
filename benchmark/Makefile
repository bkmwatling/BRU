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


# Preprocessing

INTERMEDIATE_DATASET_DIR := $(RAW_DATASET_DIR)/intermediate
COMPILABLE_ALL_REGEX_DATASET := \
	$(INTERMEDIATE_DATASET_DIR)/compilable_all_regexes.jsonl
COMPILABLE_SL_REGEX_DATASET := \
	$(INTERMEDIATE_DATASET_DIR)/compilable_sl_regexes.jsonl

.PHONY: preprocess
preprocess: preprocess-regexes preprocess-sl-regexes

.PHONY: preprocess-regexes
preprocess-regexes: $(ALL_REGEX_DATASET)

.PHONY: preprocess-sl-regexes
preprocess-sl-regexes: $(SL_REGEX_DATASET)

$(COMPILABLE_ALL_REGEX_DATASET): $(RAW_ALL_REGEX_DATASET) | $(VENV) $(INTERMEDIATE_DATASET_DIR)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/filter_compilable.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log

$(COMPILABLE_SL_REGEX_DATASET): $(RAW_SL_REGEX_DATASET) | $(VENV) $(INTERMEDIATE_DATASET_DIR)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/filter_compilable.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log

$(ALL_REGEX_DATASET): $(COMPILABLE_ALL_REGEX_DATASET) | $(VENV) $(INTERMEDIATE_DATASET_DIR)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/preprocess_all_regexes.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log

$(SL_REGEX_DATASET): $(COMPILABLE_SL_REGEX_DATASET) | $(VENV) $(INTERMEDIATE_DATASET_DIR)
	make $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))
	$(PYTHON) $(RAW_DATASET_DIR)/preprocess_sl_regexes.py $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$(basename $(notdir $@))/log.log


REGEX_TYPES := all sl
CONSTRUCTIONS := thompson glushkov
MEMO_SCHEMES := none cn in
SCHEDULERS := spencer lockstep
MATCHING_TYPES := full partial

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


# Benchmarking
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
benchmark: \
	benchmark-sl-full \
	benchmark-all-full \
	benchmark-sl-partial \
	benchmark-all-partial

.PHONY: benchmark-sl-full
benchmark-sl-full: $(BENCHMARK_SL_FULL)

.PHONY: benchmark-all-full
benchmark-all-full: $(BENCHMARK_ALL_FULL)

.PHONY: benchmark-sl-partial
benchmark-sl-partial: $(BENCHMARK_SL_PARTIAL)

.PHONY: benchmark-all-partial
benchmark-all-partial: $(BENCHMARK_ALL_PARTIAL)


# Statistics
STATISTICS_DIR := $(DATA_DIR)/statistics

# $(STATISTICS_DIR)/statistics-{all,sl}-{thompson,glushkov}-{none,cn,in}.jsonl
STATISTICS := $(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(foreach memo_scheme,$(MEMO_SCHEMES),\
			$(STATISTICS_DIR)/statistics-$(regex_type)-$(construction)-$(memo_scheme).jsonl\
		)\
	)\
)

# $(1) = regex_type, $(2) = construction, $(3) = memo_scheme
define statistics_template
$(STATISTICS_DIR)/statistics-$(1)-$(2)-$(3).jsonl: $(DATASET_DIR)/$(1)_regexes.jsonl | $(STATISTICS_DIR) $(VENV)
	make $(LOGS_DIR)/$(TIMESTAMP)-$$(basename $$(notdir $$@))
	$(PYTHON) src/regex_statistics.py --construction $(2) --memo-scheme $(3) $$< $$@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-$$(basename $$(notdir $$@))/log.log
endef

# generate statistics target recipes
$(foreach regex_type,$(REGEX_TYPES),\
	$(foreach construction,$(CONSTRUCTIONS),\
		$(foreach memo_scheme,$(MEMO_SCHEMES),\
			$(eval $(call statistics_template,$(regex_type),$(construction),$(memo_scheme)))\
		)\
	)\
)

.PHONY: statistics
statistics: $(STATISTICS)


# Analysis
ANALYSIS_DIR := $(DATA_DIR)/analysis

# Analysis Steps
STEPS_SL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,full,tex)
STEPS_ALL_FULL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,full,tex)
STEPS_SL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),steps,sl,partial,tex)
STEPS_ALL_PARTIAL := $(call target_variables_template,$(ANALYSIS_DIR),steps,all,partial,tex)

$(ANALYSIS_DIR)/steps-%.tex: $(BENCHMARK_DIR)/benchmark-%.jsonl | $(VENV)
	@mkdir -p $(ANALYSIS_DIR)
	$(PYTHON) src/analysis_steps.py \
		--regex-type $(word 1,$(subst -, ,$*)) $< > $@ || rm $@

.PHONY: analysis-steps
analysis-steps: \
	analysis-steps-all-full \
	analysis-steps-sl-full \
	analysis-steps-all-partial \
	analysis-steps-sl-partial

.PHONY: analysis-steps-all-full
analysis-steps-all-full: $(STEPS_ALL_FULL)

.PHONY: analysis-steps-sl-full
analysis-steps-sl-full: $(STEPS_SL_FULL)

.PHONY: analysis-steps-all-partial
analysis-steps-all-partial: $(STEPS_ALL_PARTIAL)

.PHONY: analysis-steps-sl-partial
analysis-steps-sl-partial: $(STEPS_SL_PARTIAL)

# Analysis Statistics
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
	vim $$(git ls-files) $$(git ls-files --others --exclude-standard)
