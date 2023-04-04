CC = clang
CFLAGS = -pedantic -Wall -Wextra
DFLAGS = # -DDEBUG
FLAGS = $(CFLAGS) $(DFLAGS)
RM = rm -rf

PARSER = parse
COMPILER = compile
SRVM = srvm

SRC = src
OBJ = obj
BIN = bin

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
BINS = $(BIN)/$(PARSER) $(BIN)/$(COMPILER) $(BIN)/$(SRVM)

PARSER_OBJS = $(filter-out $(patsubst $(BIN)/%,$(OBJ)/%.o,$(BINS)),$(OBJS))
COMPILER_OBJS = $(filter-out $(OBJ)/$(SRVM).o $(OBJ)/$(COMPILER).o,$(OBJS))
SRVM_OBJS = $(filter-out $(OBJ)/$(SRVM).o,$(OBJS))


all: $(PARSER) $(COMPILER) $(SRVM)

$(SRVM): $(SRVM_OBJS) $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(SRC)/$@.c $(SRVM_OBJS)

$(COMPILER): $(OBJS) $(COMPILER_BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(SRC)/$@.c $(COMPILER_OBJS)

$(PARSER): $(OBJS) $(PARSER_BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(SRC)/$@.c $(PARSER_OBJS)

$(OBJ)/%.o: $(SRC)/%.c $(OBJ)
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

.PHONY: all $(SRVM) $(COMPILER) $(PARSER) clean cleanobj cleanbin
