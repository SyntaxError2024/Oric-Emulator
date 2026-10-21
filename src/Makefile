CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -municode -s -static -municode -mwindows
TARGET = oric_atmos.exe
SRC = common.c cpu.c memory.c via.c ay38912.c keyboard.c tape.c video.c main.c
OBJ = $(SRC:.c=.o)
LDLIBS = -lgdi32 -lwinmm -lshell32

$(TARGET): $(OBJ)
	-taskkill /F /IM "$(TARGET)"
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
