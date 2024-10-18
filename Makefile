##
# Symbolic Regular Expression with counters Virtual Machine
#
# @author Brendan Watling
# @file bru.c
# @version 0.1

# path
INSTALL_PREFIX := ~/.local

# compiler flags
DEBUG          := -ggdb -gdwarf-4
OPTIMISE       := -O0
WARNING        := -Wall -Wextra -Wno-variadic-macros -Wno-overlength-strings \
				  -Wno-gnu-zero-variadic-macro-arguments -pedantic
EXTRA          := -std=c11
INCLUDE         = $(addprefix -I,$(INCLUDEDIR))
STCOPT         := -DSTC_UTF_DISABLE_SV
CFLAGS          = $(DEBUG) $(OPTIMISE) $(WARNING) $(EXTRA) $(INCLUDE) $(STCOPT)
DFLAGS         := # -DBRU_DEBUG -DBRU_BENCHMARK

# commands
CC             := clang
RM             := rm -f
COMPILE         = $(CC) $(CFLAGS) $(DFLAGS)
INSTALL        := install --preserve-timestamps

# directories
LOCALBIN       := $(INSTALL_PREFIX)/bin
SRCDIR         := src
LIBDIR         := lib
BINDIR         := bin
STCDIR         := $(LIBDIR)/stc
REDIR          := $(SRCDIR)/re
FADIR          := $(SRCDIR)/fa
VMDIR          := $(SRCDIR)/vm
INCLUDEDIR     := $(STCDIR)/include

# files
BRU_EXE        := bru
EXE            := $(BRU_EXE)

BRU_SRC        := $(SRCDIR)/$(BRU_EXE).c
EXE_SRC        := $(BRU_SRC)

FATP_SRC       := slice.c string_view.c vec.c
UTIL_SRC       := argparser.c utf.c
STC_SRC        := $(addprefix $(STCDIR)/src/fatp/, $(FATP_SRC)) \
				  $(addprefix $(STCDIR)/src/util/, $(UTIL_SRC))

RE_SRC         := $(wildcard $(REDIR)/*.c)
FA_SRC         := $(wildcard $(FADIR)/*.c) \
				  $(wildcard $(FADIR)/constructions/*.c) \
				  $(wildcard $(FADIR)/transformers/*.c)
VM_SRC         := $(wildcard $(VMDIR)/*.c) \
				  $(wildcard $(VMDIR)/thread_managers/*.c) \
				  $(wildcard $(VMDIR)/thread_managers/schedulers/*.c)

SRC            := $(filter-out $(EXE_SRC), $(wildcard $(SRCDIR)/*.c)) \
				  $(STC_SRC) $(RE_SRC) $(FA_SRC) $(VM_SRC)
OBJ            := $(SRC:.c=.o)

### RULES ######################################################################

# executables

$(BRU_EXE): $(BRU_SRC) $(OBJ) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

# units

%.o: %.c
	$(COMPILE) -c -o $@ $<

# BINDIR

$(BINDIR):
	mkdir -p $(BINDIR)

# LOCALBIN

$(LOCALBIN):
	mkdir -p $(LOCALBIN)

### PHONY TARGETS ##############################################################

all: $(EXE)

# Install all BRU-related binaries in the local bin.
install: all | $(LOCALBIN)
	$(INSTALL) $(addprefix $(BINDIR)/, $(EXE)) $(LOCALBIN)

# Remove all BRU-related binaries from the local bin.
uninstall:
	$(RM) $(addprefix $(LOCALBIN)/, $(EXE))

clean: cleanobj cleanbin

cleanobj:
	$(RM) $(OBJ)

cleanbin:
	$(RM) $(addprefix $(BINDIR)/, $(EXE))

.PHONY: all install uninstall clean cleanobj cleanbin

# end
