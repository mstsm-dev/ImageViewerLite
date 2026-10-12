CC := gcc
WINDRES := windres
TARGET := dist/LiteViewer.exe
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -DUNICODE -D_UNICODE -DCOBJMACROS -D_WIN32_WINNT=0x0A00 -Iinclude
LDFLAGS := -mwindows -Wl,--subsystem,windows
LDLIBS := -luser32 -lgdi32 -lmsimg32 -lcomdlg32 -lcomctl32 -lshell32 -lshlwapi -lole32 -lwindowscodecs -luuid -lcomctl32 -lm
all: $(TARGET)
$(TARGET): $(OBJECTS) build/resource.o
	@mkdir -p dist
	$(CC) $(OBJECTS) build/resource.o $(LDFLAGS) $(LDLIBS) -o $@
build/%.o: src/%.c include/viewer.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/resource.o: src/resource.rc
	@mkdir -p build
	$(WINDRES) -i $< -o $@
clean:
	rm -rf build dist/*.exe
.PHONY: all clean
