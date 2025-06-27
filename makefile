CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c11 -g

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests
SOURCES = $(wildcard $(SRC_DIR)/*.c) main.c
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/server

# Library objects (excluding main.c for tests)
LIB_SOURCES = $(wildcard $(SRC_DIR)/*.c)
LIB_OBJECTS = $(LIB_SOURCES:%.c=$(OBJ_DIR)/%.o)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR) $(OBJ_DIR)/$(SRC_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Generic test target: make test-<component>
# Example: make test-http will compile and run tests/test_http.c
test-%: $(BIN_DIR)/test_% | $(BIN_DIR)
	./$<

$(BIN_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR) $(BIN_DIR) $(OBJ_DIR)/$(SRC_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clean-tests:
	rm -f $(BIN_DIR)/test_*

.PHONY: all clean clean-tests test-%
