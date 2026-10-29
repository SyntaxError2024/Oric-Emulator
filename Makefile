CC = gcc
LDFLAGS= -municode -mwindows -s -static
CFLAGS = -O2
LDLIBS = -lgdi32 -lwinmm -lshell32
TARGET = oric_atmos.exe
SRC_DIR := src
OBJ_DIR := obj

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

build: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(TARGET) $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) programme

.PHONY: clean