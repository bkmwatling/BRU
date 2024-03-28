# Analysis
ANALYSIS_DIR := $(DATA_DIR)/analysis

include makefiles/analysis_steps.mk
include makefiles/analysis_memo_size.mk

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

.PHONY: analyze-statistics
analyze-statistics: $(ANALYSIS_STATS_SL)
