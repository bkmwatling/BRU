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
