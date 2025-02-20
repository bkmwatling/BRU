##
# Brendan's Regex Utility (BRU)
#
# @author Brendan Watling
# @file bru.c
# @version 0.1


# path
INSTALL_PREFIX := ~/.local

# commands
CC             := clang
RM             := rm -f
COMPILE         = $(CC) $(CFLAGS) $(DFLAGS)
INSTALL        := install --preserve-timestamps

# compiler flags
BUILD_TYPE     ?= Debug
ifeq ($(BUILD_TYPE), Debug)
    DEBUG      := -ggdb -gdwarf-4
else ifeq ($(BUILD_TYPE), Release)
    DEBUG      :=
endif

# enable Address Sanitizer by having `asan` in the ENABLE variable list
# e.g. make ENABLE=asan ...  OR  make ENABLE=...,asan ...
ifneq ($(findstring asan, $(ENABLE)),)
    # NOTE: run executable with `run_with_asan.sh` to use Address Sanitizer,
    #       which works by giving command to run after the script,
    #       e.g. ./run_with_asan.sh ./bin/bru ...
    ifeq ($(CC), gcc)
        DEBUG      += -fsanitize=address
    else ifeq ($(CC), clang)
        DEBUG      += -fsanitize=address -shared-libasan -ferror-limit=1
    endif
endif

OPTIMISE       := -O0
WARNING        := -Wall -Wextra -Wswitch-enum -Wpedantic
ifeq ($(CC), gcc) # GCC gives warnings for empty variadic macros with -Wpedantic
    WARNING    += -Wno-unused-value
    EXTRA      += -std=gnu11
else ifeq ($(CC), clang)
    WARNING    += -Wno-gnu-zero-variadic-macro-arguments
    EXTRA      += -std=c11
endif
EXTRA          += -fPIC
INCLUDE         = $(addprefix -I, $(INCLUDEDIRS))
STCOPT         := -DSTC_UTF_DISABLE_SV
CFLAGS          = $(DEBUG) $(OPTIMISE) $(WARNING) $(EXTRA) $(INCLUDE) $(STCOPT)
DFLAGS         += #-DBRU_DEBUG -DBRU_BENCHMARK

# directories
LOCALBIN       := $(INSTALL_PREFIX)/bin
SRCDIR         := src
TEST_SRCDIR    := testing/src
LIBDIR         := lib
BINDIR         := bin
STCDIR         := $(LIBDIR)/stc
REDIR          := $(SRCDIR)/re
FADIR          := $(SRCDIR)/fa
VMDIR          := $(SRCDIR)/vm
INCLUDEDIRS    += include $(STCDIR)/include

# files
BRU_EXE        := bru
TEST_EXE       := test_bru
EXE            := $(BRU_EXE) $(TEST_EXE)

BRU_SRC        := $(SRCDIR)/$(BRU_EXE).c
TEST_BRU_SRC   := $(TEST_SRCDIR)/$(TEST_EXE).c
EXE_SRC        := $(BRU_SRC) $(TEST_BRU_SRC)

FATP_SRC       := str_view.c vec.c
UTIL_SRC       := argparser.c utf.c
STC_SRC        := $(addprefix $(STCDIR)/src/fatp/, $(FATP_SRC)) \
                  $(addprefix $(STCDIR)/src/util/, $(UTIL_SRC))

RE_SRC         := $(wildcard $(REDIR)/*.c)
FA_SRC         := $(wildcard $(FADIR)/*.c) \
                  $(wildcard $(FADIR)/constructions/*.c) \
                  $(wildcard $(FADIR)/transformers/*.c)
VM_SRC         := $(wildcard $(VMDIR)/*.c) \
                  $(wildcard $(VMDIR)/thread_managers/*.c) \
                  $(wildcard $(VMDIR)/thread_managers/schedulers/*.c) \
                  $(wildcard $(VMDIR)/compilers/*.c)

SRC            := $(filter-out $(EXE_SRC), $(wildcard $(SRCDIR)/*.c)) \
                  $(STC_SRC) $(RE_SRC) $(FA_SRC) $(VM_SRC)
TEST_SRC       := $(filter-out $(TEST_BRU_SRC), $(wildcard $(TEST_SRCDIR)/*.c))
OBJ            := $(SRC:.c=.o)
TEST_OBJ       := $(TEST_SRC:.c=.o)

### RULES ######################################################################

# executables

$(BRU_EXE): $(BRU_SRC) $(OBJ) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

$(TEST_EXE): $(TEST_BRU_SRC) $(OBJ) $(TEST_OBJ) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^ 

# units

%.o: %.c
	$(COMPILE) -c -o $@ $<

# directories

$(BINDIR):
	mkdir -p $(BINDIR)

$(LOCALBIN):
	mkdir -p $(LOCALBIN)

### PHONY TARGETS ##############################################################

all: $(EXE)

test: $(TEST_EXE)
	$(BINDIR)/$(TEST_EXE) testing/tests/rxspencer-all-bru-configs.tsv \
                          testing/results.log

# Install all BRU-related binaries in the local bin.
install: all | $(LOCALBIN)
	$(INSTALL) $(addprefix $(BINDIR)/, $(EXE)) $(LOCALBIN)

# Remove all BRU-related binaries from the local bin.
uninstall:
	$(RM) $(addprefix $(LOCALBIN)/, $(EXE))

clean: cleanobj cleanbin

cleanobj:
	$(RM) $(OBJ) $(TEST_OBJ)

cleanbin:
	$(RM) $(addprefix $(BINDIR)/, $(EXE))

.PHONY: all test install uninstall clean cleanobj cleanbin

# end
