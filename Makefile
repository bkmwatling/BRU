CC = clang
CFLAGS = -pedantic -Wall -Wextra
DFLAGS = # -DDEBUG
FLAGS = $(CFLAGS) $(DFLAGS)
RM = rm -f

PARSE = parse
COMPILE = compile
MATCH = match

SRC = src
OBJ = obj
BIN = bin

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(filter-out $(BIN_SRCS),$(SRCS)))
BINS = $(BIN)/$(PARSE) $(BIN)/$(COMPILE) $(BIN)/$(MATCH)
BIN_SRCS = $(BINS:$(BIN)/%=$(SRC)/%.c)

PARSE_OBJS = $(filter-out $(OBJ)/compiler.o $(OBJ)/srvm.o,$(OBJS))


all: $(PARSE) $(COMPILE) $(MATCH)

$(MATCH): $(OBJS) | $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(SRC)/$@.c $(OBJS)

$(COMPILE): $(OBJS) | $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(SRC)/$@.c $(OBJS)

$(PARSE): $(PARSE_OBJS) | $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(SRC)/$@.c $(PARSE_OBJS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(FLAGS) -c -o $@ $<


$(BIN):
	mkdir -p $(BIN)

$(OBJ):
	mkdir -p $(OBJ)


clean: cleanobj cleanbin

cleanobj:
	$(RM) $(OBJS)

cleanbin:
	$(RM) $(BINS)

.PHONY: all $(SRVM) $(COMPILE) $(PARSE) clean cleanobj cleanbin
