ANALYSIS_STATISTICS_DIR := $(ANALYSIS_DIR)/statistics
ANALYSIS_STATISTICS := $(addprefix $(ANALYSIS_STATISTICS_DIR)/, \
		$(shell echo {all,sl}-{thompson,flat}-{cn,in,none}.tex))
.PHONY: analyze-statistics
analyze-statistics: $(ANALYSIS_STATISTICS)
include makefiles/analysis_statistics.mk


ANALYSIS_STEP_DIR := $(ANALYSIS_DIR)/step
ANALYSIS_STEP_ALL := $(addprefix $(ANALYSIS_STEP_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
ANALYSIS_STEP_SL := $(addprefix $(ANALYSIS_STEP_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
.PHONY: analyze-step
analyze-step: analyze-step-all analyze-step-sl
include makefiles/analysis_step.mk


ANALYSIS_MEMO_SIZE_DIR := $(ANALYSIS_DIR)/memo_size
ANALYSIS_MEMO_SIZE_ALL := $(addprefix $(ANALYSIS_MEMO_SIZE_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in}.tex))
ANALYSIS_MEMO_SIZE_SL := $(addprefix $(ANALYSIS_MEMO_SIZE_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in}.tex))
.PHONY: analyze-memo-size
analyze-memo-size: analyze-memo-size-all
include makefiles/analysis_memo_size.mk


ANALYSIS_STATE_DIR := $(ANALYSIS_DIR)/state
ANALYSIS_STATE_ALL := $(addprefix $(ANALYSIS_STATE_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
ANALYSIS_STATE_SL := $(addprefix $(ANALYSIS_STATE_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
.PHONY: analyze-state
analyze-state: analyze-state-all
include makefiles/analysis_state.mk

ANALYSIS_DIRS := \
		 $(ANALYSIS_STATISTICS_DIR) \
		 $(ANALYSIS_STEP_DIR) \
		 $(ANALYSIS_MEMO_SIZE_DIR) \
		 $(ANALYSIS_STATE_DIR)

$(ANALYSIS_DIRS):
	@mkdir -p $@
