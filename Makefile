##
# Symbolic Regular Expression with counters Virtual Machine
#
# @author Brendan Watling
# @file match.c
# @version 0.1

# compiler flags
DEBUG       := -ggdb -gdwarf-4
OPTIMISE    := -O0
WARNINGS    := -Wall -Wextra -Wno-variadic-macros -Wno-overlength-strings \
               -pedantic
STC_OPT     := -DSTC_UTF_DISABLE_SV
CFLAGS      := $(DEBUG) $(OPTIMISE) $(WARNINGS) $(STC_OPT)
DFLAGS      := # -DDEBUG

# commands
CC          := clang
RM          := rm -f
COMPILE     := $(CC) $(CFLAGS) $(DFLAGS)

# directories
SRCDIR      := src
BINDIR      := bin

# files
BRU_EXE     := bru
EXE         := $(BRU_EXE)

BRU_SRC     := $(SRCDIR)/$(BRU_EXE).c
EXE_SRC     := $(BRU_SRC)

STC_SRC     := $(SRCDIR)/stc/fatp/slice.c $(SRCDIR)/stc/fatp/string_view.c \
               $(SRCDIR)/stc/fatp/vec.c $(SRCDIR)/stc/util/args.c \
               $(SRCDIR)/stc/util/utf.c
WALKERS_SRC := $(wildcard $(SRCDIR)/walkers/*.c) \
               $(wildcard $(SRCDIR)/walkers/thompson/*.c) \
               $(wildcard $(SRCDIR)/walkers/glushkov/*.c)
SRC         := $(filter-out $(EXE_SRC), $(wildcard $(SRCDIR)/*.c)) \
               $(STC_SRC) $(WALKERS_SRC)
OBJ         := $(SRC:.c=.o)

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

### PHONY TARGETS ##############################################################

all: $(EXE)

clean: cleanobj cleanbin

cleanobj:
	$(RM) $(OBJ)

cleanbin:
	$(RM) $(addprefix $(BINDIR)/, $(EXE))

.PHONY: all clean cleanobj cleanbin

# end
