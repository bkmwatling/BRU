VENV := .venv
BRU_PATH := $(shell dirname $(shell pwd))/bin
PYTHON := $(VENV)/bin/python
PIP := $(VENV)/bin/pip
export PATH := $(PATH):$(BRU_PATH)
LOGS_DIR := logs
TIMESTAMP := $(shell date +%Y%m%d-%H%M%S)

RAW_DATASET_DIR := data-build
RAW_REGEX_DATASET := $(RAW_DATASET_DIR)/all_regexes.jsonl
RAW_SL_REGEX_DATASET := $(RAW_DATASET_DIR)/sl_regexes.jsonl
DATASET_DIR := data
REGEX_DATASET := $(DATASET_DIR)/all_regexes.jsonl
SL_REGEX_DATASET := $(DATASET_DIR)/sl_regexes.jsonl
DIRS := $(DATASET_DIR) $(RAW_DATASET_DIR) $(LOGS_DIR)

.DEFAULT:

.PHONY: preprocess
preprocess: preprocess-regexes preprocess-sl-regexes

.PHONY: preprocess-regexes
preprocess-regexes: $(REGEX_DATASET)

.PHONY: preprocess-sl-regexes
preprocess-sl-regexes: $(SL_REGEX_DATASET)

$(DIRS):
	mkdir -p $@

$(REGEX_DATASET): $(RAW_REGEX_DATASET) | $(VENV) $(DIRS)
	make $(LOGS_DIR)/$(TIMESTAMP)-preprocess-regexes
	$(PYTHON) data-build/preprocess.py regex $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-preprocess-regexes/log.log

$(SL_REGEX_DATASET): $(RAW_SL_REGEX_DATASET) | $(VENV) $(DIRS)
	make $(LOGS_DIR)/$(TIMESTAMP)-preprocess-sl-regexes
	$(PYTHON) data-build/preprocess.py sl-regex $< $@ \
		2> $(LOGS_DIR)/$(TIMESTAMP)-preprocess-sl-regexes/log.log

$(VENV): requirements.txt
	python3 -m venv .venv
	touch $(VENV)
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements.txt


.PHONY: clean
clean:
	rm -rf $(VENV)
	rm -rf $(LOGS_DIR)

.PHONY: vim
vim:
	vim $$(git ls-files) $$(git ls-files --others --exclude-standard)
