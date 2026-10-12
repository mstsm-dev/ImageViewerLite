# ImageViewerLite — MSYS2 UCRT64 用 Makefile
#
#   make            リリースビルド（dist/ImageViewerLite.exe）
#   make DEBUG=1    デバッグ情報付き・最適化なし
#   make dist       EXE と INI を dist/ にそろえる
#   make clean      生成物を削除
#
# クロスコンパイル例: make CROSS=x86_64-w64-mingw32-

CROSS   ?=
CC      := $(CROSS)gcc
WINDRES := $(CROSS)windres

APP     := ImageViewerLite
TARGET  := dist/$(APP).exe
INI     := $(APP).ini

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
RES_OBJ := build/resource.o
DEPS    := $(OBJECTS:.o=.d)

DEFINES  := -DUNICODE -D_UNICODE -DCOBJMACROS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00
WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wshadow -Wstrict-prototypes
CFLAGS   := -std=c11 $(WARNINGS) $(DEFINES) -Iinclude -MMD -MP
LDFLAGS  := -municode -mwindows
LDLIBS   := -luser32 -lgdi32 -lmsimg32 -lcomdlg32 -lcomctl32 -lshell32 -lshlwapi \
            -lole32 -lwindowscodecs -ld2d1 -luuid -lm

ifeq ($(DEBUG),1)
  CFLAGS += -O0 -g
else
  CFLAGS  += -O2
  LDFLAGS += -s
endif

.PHONY: all dist clean

all: $(TARGET)

# -municode が必要：エントリーポイントが wWinMain のため
$(TARGET): $(OBJECTS) $(RES_OBJ) | dist-dir
	$(CC) $(OBJECTS) $(RES_OBJ) $(LDFLAGS) $(LDLIBS) -o $@

build/%.o: src/%.c | build-dir
	$(CC) $(CFLAGS) -c $< -o $@

# アイコン・マニフェスト・版数を変えたときもリソースを作り直す
$(RES_OBJ): res/resource.rc res/$(APP).ico res/manifest.xml include/resource.h include/version.h | build-dir
	$(WINDRES) --codepage=65001 $(DEFINES) -Iinclude -Ires -i $< -o $@

dist: $(TARGET)
	cp $(INI) dist/

build-dir:
	@mkdir -p build

dist-dir:
	@mkdir -p dist

clean:
	rm -rf build dist

-include $(DEPS)
