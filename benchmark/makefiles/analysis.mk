.PHONY: analyze
analyze: analyze-statistics analyze-step analyze-memo-size


ANALYSIS_STATISTICS_DIR := $(ANALYSIS_DIR)/statistics
ANALYSIS_STATISTICS := $(addprefix $(ANALYSIS_STATISTICS_DIR)/, \
		$(shell echo {all,sl}-{thompson,flat}-{cn,in,none}.tex))
include makefiles/analysis_statistics.mk


ANALYSIS_STEP_DIR := $(ANALYSIS_DIR)/step
ANALYSIS_STEP_ALL := $(addprefix $(ANALYSIS_STEP_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
ANALYSIS_STEP_SL := $(addprefix $(ANALYSIS_STEP_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
include makefiles/analysis_step.mk


ANALYSIS_MEMO_SIZE_DIR := $(ANALYSIS_DIR)/memo_size
ANALYSIS_MEMO_SIZE_ALL := $(addprefix $(ANALYSIS_MEMO_SIZE_DIR)/all-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
ANALYSIS_MEMO_SIZE_SL := $(addprefix $(ANALYSIS_MEMO_SIZE_DIR)/sl-, \
		$(shell echo {full,partial}-{thompson,flat}-spencer-{cn,in,none}.tex) \
		$(shell echo {full,partial}-{thompson,flat}-lockstep-none.tex))
include makefiles/analysis_memo_size.mk
