##
# Symbolic Regular Expression with counters Virtual Machine
#
# @author Brendan Watling
# @file match.c
# @version 0.1

# compiler flags
DEBUG       := -ggdb
OPTIMISE    := -O0
WARNINGS    := -Wall -Wextra -Wno-variadic-macros -Wno-overlength-strings \
               -pedantic
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
EXE         := $(MATCH_EXE) $(COMPILE_EXE) $(PARSE_EXE)

MATCH_SRC   := $(SRCDIR)/$(MATCH_EXE).c
COMPILE_SRC := $(SRCDIR)/$(COMPILE_EXE).c
PARSE_SRC   := $(SRCDIR)/$(PARSE_EXE).c
EXE_SRC     := $(MATCH_SRC) $(COMPILE_SRC) $(PARSE_SRC)

SRC         := $(filter-out $(EXE_SRC), $(wildcard $(SRCDIR)/*.c)) \
               $(SRCDIR)/stc/util/args.c
OBJ         := $(SRC:.c=.o)
PARSE_OBJ   := $(filter-out $(addprefix $(SRCDIR)/, compiler.o srvm.o), $(OBJ))

### RULES ######################################################################

# executables

$(MATCH_EXE): $(MATCH_SRC) $(OBJ) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

$(COMPILE_EXE): $(COMPILE_SRC) $(OBJ) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

$(PARSE_EXE): $(PARSE_SRC) $(PARSE_OBJ) | $(BINDIR)
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
