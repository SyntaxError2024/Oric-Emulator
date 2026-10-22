CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -municode -s -static -mwindows
LDLIBS = -lgdi32 -lwinmm -lshell32

TARGET = oric.exe
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

$(TARGET): $(OBJ)
	-taskkill /F /IM "$(TARGET)"
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

obj/%.o: src/%.c
	if not exist obj mkdir obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(OBJ) $(TARGET) 2>NUL

.PHONY: clean