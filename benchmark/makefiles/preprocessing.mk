# Raw dataset directories
RAW_DATASET_DIR := data-build
RAW_ALL_REGEX_DATASET := $(RAW_DATASET_DIR)/all_regexes.jsonl
RAW_SL_REGEX_DATASET := $(RAW_DATASET_DIR)/sl_regexes.jsonl

INTERMEDIATE_DATASET_DIR := $(RAW_DATASET_DIR)/intermediate

.PHONY: preprocess
preprocess: preprocess-all-regexes preprocess-sl-regexes

.PHONY: preprocess-all-regexes
preprocess-all-regexes: $(ALL_REGEX_DATASET)

.PHONY: preprocess-sl-regexes
preprocess-sl-regexes: $(SL_REGEX_DATASET)

.PRECIOUS: $(INTERMEDIATE_DATASET_DIR)/compilable_%_regexes.jsonl
$(INTERMEDIATE_DATASET_DIR)/compilable_%_regexes.jsonl: \
		$(RAW_DATASET_DIR)/%_regexes.jsonl \
		| $(VENV) $(INTERMEDIATE_DATASET_DIR) $(LOGS_DIR)
	$(PYTHON) $(RAW_DATASET_DIR)/filter_compilable.py $< $@ \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log

.PRECIOUS: $(DATASET_DIR)/%_regexes.jsonl
$(DATASET_DIR)/%_regexes.jsonl: \
		$(INTERMEDIATE_DATASET_DIR)/compilable_%_regexes.jsonl \
		| $(VENV) $(DATASET_DIR)
	$(PYTHON) $(RAW_DATASET_DIR)/preprocess_$*_regexes.py $< $@ \
		2> $(LOGS_DIR)/$(basename $(notdir $@)).log

$(INTERMEDIATE_DATASET_DIR):
	mkdir -p $@

