CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c11 -g
RELEASE_CFLAGS = -Iinclude -Wall -Wextra -std=c11 -O3 -march=native -flto -DNDEBUG -s
DEBUG_CFLAGS = $(CFLAGS) -DDEBUG

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests

SOURCES = $(wildcard $(SRC_DIR)/*.c) main.c
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/server

LIB_SOURCES = $(wildcard $(SRC_DIR)/*.c)
LIB_OBJECTS = $(LIB_SOURCES:%.c=$(OBJ_DIR)/%.o)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR) $(OBJ_DIR)/$(SRC_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

debug: $(OBJECTS) | $(BIN_DIR)
	$(CC) $(DEBUG_CFLAGS) -o $(EXECUTABLE) $^ -lpthread

.PHONY: release
release: clean
	$(MAKE) CFLAGS="$(RELEASE_CFLAGS)" all

test-%: $(BIN_DIR)/test_% | $(BIN_DIR)
	./$<

$(BIN_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

$(OBJ_DIR) $(BIN_DIR) $(OBJ_DIR)/$(SRC_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clean-tests:
	rm -f $(BIN_DIR)/test_*

.PHONY: all clean clean-tests test-%
