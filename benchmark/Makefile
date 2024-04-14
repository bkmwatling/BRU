VENV := .venv
BRU_PATH := $(shell dirname $(shell pwd))/bin
PYTHON := $(VENV)/bin/python
PIP := $(VENV)/bin/pip
export PATH := $(PATH):$(BRU_PATH)

DATA_DIR := data
TIMESTAMP := $(shell date +%Y%m%d%H%M%S)
LOGS_PREFIX := logs
LOGS_DIR := $(LOGS_PREFIX)/$(TIMESTAMP)

REGEX_TYPES := all sl
CONSTRUCTIONS := thompson flat
MEMO_SCHEMES := none cn in
SCHEDULERS := spencer lockstep
MATCHING_TYPES := full partial

.ONESHELL:

.DEFAULT: default
default: analyze


DATASET_DIR := $(DATA_DIR)/polyglot
ALL_REGEX_DATASET := $(DATASET_DIR)/all_regexes.jsonl
SL_REGEX_DATASET := $(DATASET_DIR)/sl_regexes.jsonl
include makefiles/preprocessing.mk


BENCHMARK_DIR := $(DATA_DIR)/benchmark
BENCHMARK_ALL_FULL := $(addprefix $(BENCHMARK_DIR)/all-full-, \
		$(shell echo {thompson,flat}-spencer-{cn,in,none}.jsonl) \
		$(shell echo {thompson,flat}-lockstep-none.jsonl))
BENCHMARK_ALL_PARTIAL := $(addprefix $(BENCHMARK_DIR)/all-partial-, \
		$(shell echo {thompson,flat}-spencer-{cn,in,none}.jsonl) \
		$(shell echo {thompson,flat}-lockstep-none.jsonl))
BENCHMARK_SL_FULL := $(addprefix $(BENCHMARK_DIR)/sl-full-, \
		$(shell echo {thompson,flat}-spencer-{cn,in,none}.jsonl) \
		$(shell echo {thompson,flat}-lockstep-none.jsonl))
BENCHMARK_SL_PARTIAL := $(addprefix $(BENCHMARK_DIR)/sl-partial-, \
		$(shell echo {thompson,flat}-spencer-{cn,in,none}.jsonl) \
		$(shell echo {thompson,flat}-lockstep-none.jsonl))
include makefiles/benchmark.mk


STATISTICS_DIR := $(DATA_DIR)/statistics
STATISTICS := $(addprefix $(STATISTICS_DIR)/, \
		$(shell echo {all,sl}-{thompson,flat}-{cn,in,none}.jsonl))
include makefiles/statistics.mk


STEP_DIR := $(DATA_DIR)/step
STEP_ALL := $(addprefix $(STEP_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.jsonl) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.jsonl))
STEP_SL := $(addprefix $(STEP_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.jsonl) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.jsonl))
include makefiles/step.mk


MEMO_SIZE_DIR := $(DATA_DIR)/memo-size
MEMO_SIZE_ALL := $(addprefix $(MEMO_SIZE_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in}.jsonl))
MEMO_SIZE_SL := $(addprefix $(MEMO_SIZE_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in}.jsonl))
include makefiles/memo_size.mk


ANALYSIS_DIR := $(DATA_DIR)/analysis
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

.PHONY: vim
vim:
	vim $$(git ls-files)

NOTEBOOK_DIR := notebooks

.PHONY: jupyter
jupyter: | $(VENV) $(LOGS_DIR)/jupyter
	$(VENV)/bin/jupyter notebook \
		--ip=0.0.0.0 --port=50000 --no-browser \
		2> $(LOGS_DIR)/jupyter/log.log

DIRS := \
	$(DATA_DIR) \
	$(DATASET_DIR) \
	$(LOGS_DIR) \
	$(BENCHMARK_DIR) \
	$(STATISTICS_DIR) \
	$(STEP_DIR) \
	$(MEMO_SIZE_DIR) \
	$(ANALYSIS_DIR)

$(DIRS):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(LOGS_PREFIX)

.PHONY: clean-all
clean-all: clean
	rm -rf $(DIRS)
	rm -rf $(VENV)

.PHONY: clean-benchmark
clean-benchmark:
	rm -rf $(BENCHMARK_DIR)

.PHONY: clean-statistics
clean-statistics:
	rm -rf $(STATISTICS_DIR)

.PHONY: clean-analysis
clean-analysis:
	rm -rf $(ANALYSIS_DIR)
