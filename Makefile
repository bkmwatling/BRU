##
# Symbolic Regular Expression with counters Virtual Machine
#
# @author Brendan Watling
# @file match.c
# @version 0.1

# compiler flags
DEBUG       := -ggdb
OPTIMISE    := -O0
WARNINGS    := -Wall -Wextra -Wno-variadic-macros -Wno-overlength-strings -pedantic
CFLAGS      := $(DEBUG) $(OPTIMISE) $(WARNINGS)
DFLAGS      := # -DDEBUG

# commands
CC          := clang
RM          := rm -f
COMPILE     := $(CC) $(CFLAGS) $(DFLAGS)

# directories
SRCDIR      := src
BINDIR      := bin

# files
MATCH_EXE   := match
COMPILE_EXE := compile
PARSE_EXE   := parse
EXES        := $(MATCH_EXE) $(COMPILE_EXE) $(PARSE_EXE)

MATCH_SRC   := $(SRCDIR)/$(MATCH_EXE).c
COMPILE_SRC := $(SRCDIR)/$(COMPILE_EXE).c
PARSE_SRC   := $(SRCDIR)/$(PARSE_EXE).c
EXE_SRCS    := $(MATCH_SRC) $(COMPILE_SRC) $(PARSE_SRC)

SRCS        := $(filter-out $(EXE_SRCS), $(wildcard $(SRCDIR)/*.c))
OBJS        := $(SRCS:.c=.o)
PARSE_OBJS  := $(filter-out $(addprefix $(SRCDIR)/, compiler.o srvm.o), $(OBJS))

### RULES ######################################################################

# executables

$(MATCH_EXE): $(MATCH_SRC) $(OBJS) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

$(COMPILE_EXE): $(COMPILE_SRC) $(OBJS) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

$(PARSE_EXE): $(PARSE_SRC) $(PARSE_OBJS) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

# units

%.o: %.c
	$(COMPILE) -c -o $@ $<

# BINDIR

$(BINDIR):
	mkdir -p $(BINDIR)

### PHONY TARGETS ##############################################################

all: $(EXES)

clean: cleanobj cleanbin

cleanobj:
	$(RM) $(OBJS)

cleanbin:
	$(RM) $(addprefix $(BINDIR)/, $(EXES))

.PHONY: all clean cleanobj cleanbin

# end
