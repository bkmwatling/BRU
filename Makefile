CC = clang
CFLAGS = -pedantic -Wall -Wextra
DFLAGS = # -DDEBUG
FLAGS = $(CFLAGS) $(DFLAGS)
RM = rm -rf

PARSER = parser
COMPILER = compiler
SRVM = srvm

SRC = src
OBJ = obj
BIN = bin

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
EXES = $(BIN)/$(PARSER) $(BIN)/$(COMPILER) $(BIN)/$(SRVM)

all: $(PARSER) $(COMPILER) $(SRVM)

$(SRVM): $(OBJS) $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(OBJS)

$(COMPILER): $(OBJS) $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(OBJS)

$(PARSER): $(OBJS) $(BIN)
	$(CC) $(FLAGS) -o $(BIN)/$@ $(OBJS)

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
	$(RM) $(EXES)

.PHONY: all $(SRVM) $(COMPILER) $(PARSER) clean cleanobj cleanbin
