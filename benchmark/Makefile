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
include makefiles/memo_size.mk
include makefiles/statistics.mk
include makefiles/analysis.mk

# Misc
$(LOGS_DIR)/%: | $(LOGS_DIR)
	@mkdir -p $@

$(VENV): requirements.txt
	python3 -m venv .venv
	touch $(VENV)
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements.txt

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
	vim $$(git ls-files)

NOTEBOOK_DIR := notebooks

.PHONY: jupyter
jupyter: | $(NOTEBOOK_DIR) $(VENV)
	$(VENV)/bin/jupyter notebook \
		--ip=0.0.0.0 --port=50000 --no-browser \
		2> $(NOTEBOOK_DIR)/log.log

DIRS := \
	$(DATA_DIR) \
	$(DATASET_DIR) \
	$(RAW_DATASET_DIR) \
	$(LOGS_DIR) \
	$(INTERMEDIATE_DATASET_DIR) \
	$(BENCHMARK_DIR) \
	$(ANALYSIS_DIR) \
	$(STATISTICS_DIR) \
	$(NOTEBOOK_DIR)

$(DIRS):
	mkdir -p $@

