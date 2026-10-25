CC = gcc
CFLAGS = -O3 -s -static -municode -mwindows
LIBS = -lgdi32 -lwinmm -lshell32
TARGET = oric_atmos.exe

SRCS = src/common.c src/cpu.c src/memory.c src/via.c src/ay38912.c src/keyboard.c src/tape.c src/video.c src/browser.c src/main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
