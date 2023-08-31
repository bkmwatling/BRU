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
PARSE_EXE   := parse
COMPILE_EXE := compile
MATCH_EXE   := match
EXES        := $(PARSE_EXE) $(COMPILE_EXE) $(MATCH_EXE)

SRCS        := $(filter-out $(EXES:%=$(SRCDIR)/%.c), $(wildcard $(SRCDIR)/*.c))
OBJS        := $(SRCS:.c=.o)
PARSE_OBJS  := $(filter-out $(addprefix $(SRCDIR)/, compiler.o srvm.o), $(OBJS))

### RULES ######################################################################

# executables

$(MATCH_EXE): $(OBJS) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $(SRCDIR)/$@.c $(OBJS)

$(COMPILE_EXE): $(OBJS) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $(SRCDIR)/$@.c $(OBJS)

$(PARSE_EXE): $(PARSE_OBJS) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $(SRCDIR)/$@.c $(PARSE_OBJS)

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

.PHONY: all $(SRVM) $(COMPILE) $(PARSE) clean cleanobj cleanbin

# end
