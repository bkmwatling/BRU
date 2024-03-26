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
