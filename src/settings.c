/*
 * settings.c — INI 設定の読み込み・保存（表駆動）
 *
 * 方針（仕様書 8章）
 *   - INI は UTF-8（BOM なし）。BOM 付きでも読み込める。
 *   - 不明なキーは無視し、不正値・範囲外の値は既定値のままにする。
 *   - 保存は一時ファイルに書いてから置き換える。
 */
#include "settings.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* 列挙型フィールドを int として読み書きするための前提 */
_Static_assert(sizeof(FitMode) == sizeof(int), "FitMode must be int-sized");
_Static_assert(sizeof(SortMode) == sizeof(int), "SortMode must be int-sized");
_Static_assert(sizeof(WindowMode) == sizeof(int), "WindowMode must be int-sized");

#define MIB ((size_t)1024 * 1024)
#define MAX_INI_BYTES (1024 * 1024)

/* ------------------------------------------------------------------------
 * キー定義表
 * ------------------------------------------------------------------------ */

typedef struct {
    const char *name;
    int value;
} NamedValue;

typedef enum {
    KEY_INT,        /* int、範囲 [min, max] */
    KEY_BOOL,       /* bool、0/1・true/false など */
    KEY_DOUBLE,     /* double、範囲 [min, max] */
    KEY_ENUM,       /* int 型の列挙。names から選ぶ */
    KEY_BOOL_NAMED, /* bool。names の value が 0/1 */
    KEY_COLOR,      /* COLORREF、#RRGGBB */
    KEY_MEGABYTES   /* size_t（バイト）、INI 上は MiB 単位、範囲 [min, max] MiB */
} KeyType;

typedef struct {
    const char *section;
    const char *key;
    KeyType type;
    size_t offset;
    const NamedValue *names;
    size_t nameCount;
    double min, max;
} KeyDef;

static const NamedValue WINDOW_MODE_NAMES[] = {
    { "Normal", VIEW_NORMAL },
    { "Simple", VIEW_SIMPLE },
};
static const NamedValue FIT_NAMES[] = {
    { "ShrinkOnly", FIT_SHRINK },
    { "FitAll",     FIT_ALL },
    { "Actual",     FIT_ACTUAL },
    { "Width",      FIT_WIDTH },
    { "Height",     FIT_HEIGHT },
};
static const NamedValue INTERPOLATION_NAMES[] = {
    { "Fast",        0 },
    { "HighQuality", 1 },
};
static const NamedValue TRANSPARENCY_NAMES[] = {
    { "Solid",        0 },
    { "Checkerboard", 1 },
};
static const NamedValue SORT_NAMES[] = {
    { "NaturalName", SORT_NATURAL },
    { "Name",        SORT_NAME },
    { "Date",        SORT_DATE },
    { "Size",        SORT_SIZE },
};

#define FIELD(f) offsetof(Settings, f)
#define NAMES(t) (t), ARRAY_COUNT(t)
#define NO_NAMES NULL, 0
#define NO_RANGE 0, 0

/* 保存時はこの順で出力する（セクションは連続させること）。 */
static const KeyDef KEY_TABLE[] = {
    { "Window",     "X",               KEY_INT,        FIELD(x),               NO_NAMES, INT_MIN, INT_MAX },
    { "Window",     "Y",               KEY_INT,        FIELD(y),               NO_NAMES, INT_MIN, INT_MAX },
    { "Window",     "Width",           KEY_INT,        FIELD(width),           NO_NAMES, 240, 16000 },
    { "Window",     "Height",          KEY_INT,        FIELD(height),          NO_NAMES, 180, 16000 },
    { "Window",     "Maximized",       KEY_BOOL,       FIELD(maximized),       NO_NAMES, NO_RANGE },
    { "Window",     "Mode",            KEY_ENUM,       FIELD(startMode),       NAMES(WINDOW_MODE_NAMES), NO_RANGE },
    { "Window",     "BackgroundColor", KEY_COLOR,      FIELD(background),      NO_NAMES, NO_RANGE },
    { "Window",     "ShowStatusBar",   KEY_BOOL,       FIELD(showStatusBar),   NO_NAMES, NO_RANGE },
    { "Display",    "FitMode",         KEY_ENUM,       FIELD(initialFit),      NAMES(FIT_NAMES), NO_RANGE },
    { "Display",    "ZoomStep",        KEY_DOUBLE,     FIELD(zoomStep),        NO_NAMES, 1.01, 4.0 },
    { "Display",    "ZoomLock",        KEY_BOOL,       FIELD(zoomLock),        NO_NAMES, NO_RANGE },
    { "Display",    "Interpolation",   KEY_BOOL_NAMED, FIELD(highQuality),     NAMES(INTERPOLATION_NAMES), NO_RANGE },
    { "Display",    "Transparency",    KEY_BOOL_NAMED, FIELD(checkerboard),    NAMES(TRANSPARENCY_NAMES), NO_RANGE },
    { "Display",    "AutoOrientation", KEY_BOOL,       FIELD(autoOrientation), NO_NAMES, NO_RANGE },
    { "Navigation", "SortBy",          KEY_ENUM,       FIELD(sort),            NAMES(SORT_NAMES), NO_RANGE },
    { "Navigation", "Descending",      KEY_BOOL,       FIELD(descending),      NO_NAMES, NO_RANGE },
    { "Navigation", "Loop",            KEY_BOOL,       FIELD(loop),            NO_NAMES, NO_RANGE },
    { "Cache",      "MaxMemoryMB",     KEY_MEGABYTES,  FIELD(maxCacheBytes),   NO_NAMES, 16, 2048 },
};

/* ------------------------------------------------------------------------
 * 既定値・パス
 * ------------------------------------------------------------------------ */

void settings_set_defaults(Settings *s)
{
    memset(s, 0, sizeof(*s));
    s->x = CW_USEDEFAULT;
    s->y = CW_USEDEFAULT;
    s->width = 1024;
    s->height = 768;
    s->maximized = false;
    s->startMode = VIEW_NORMAL;
    s->background = RGB(0x20, 0x20, 0x20);
    s->showStatusBar = true;

    s->initialFit = FIT_SHRINK;
    s->zoomStep = 1.25;
    s->zoomLock = false;
    s->highQuality = true;
    s->checkerboard = false;
    s->autoOrientation = true;

    s->sort = SORT_NATURAL;
    s->descending = false;
    s->loop = true;

    s->maxCacheBytes = 256 * MIB;
}

wchar_t *settings_default_path(void)
{
    wchar_t *exe = module_path();
    if (!exe) return NULL;

    wchar_t *slash = wcsrchr(exe, L'\\');
    if (slash) *slash = L'\0';
    wchar_t *ini = path_join(slash ? exe : L".", SETTINGS_FILE_NAME);
    free(exe);
    return ini;
}

/* ------------------------------------------------------------------------
 * 値の解析
 * ------------------------------------------------------------------------ */

static bool equals_ci(const char *a, const char *b)
{
    return _stricmp(a, b) == 0;
}

static bool parse_long(const char *text, long *out)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') return false;
    *out = value;
    return true;
}

static bool parse_bool(const char *text, bool *out)
{
    static const char *const TRUE_WORDS[] = { "1", "true", "yes", "on" };
    static const char *const FALSE_WORDS[] = { "0", "false", "no", "off" };
    for (size_t i = 0; i < ARRAY_COUNT(TRUE_WORDS); ++i) {
        if (equals_ci(text, TRUE_WORDS[i])) { *out = true; return true; }
    }
    for (size_t i = 0; i < ARRAY_COUNT(FALSE_WORDS); ++i) {
        if (equals_ci(text, FALSE_WORDS[i])) { *out = false; return true; }
    }
    return false;
}

static bool parse_named(const KeyDef *def, const char *text, int *out)
{
    for (size_t i = 0; i < def->nameCount; ++i) {
        if (equals_ci(text, def->names[i].name)) { *out = def->names[i].value; return true; }
    }
    return false;
}

static bool parse_color(const char *text, COLORREF *out)
{
    unsigned r, g, b;
    char extra;
    if (sscanf(text, "#%2x%2x%2x%c", &r, &g, &b, &extra) != 3) return false;
    *out = RGB(r, g, b);
    return true;
}

/* 1つの値を解析してフィールドに格納する。不正なら何もしない（既定値を維持）。 */
static void apply_value(Settings *s, const KeyDef *def, const char *text)
{
    void *field = (char *)s + def->offset;

    switch (def->type) {
    case KEY_INT: {
        long v;
        if (parse_long(text, &v) && v >= def->min && v <= def->max) *(int *)field = (int)v;
        break;
    }
    case KEY_BOOL: {
        bool v;
        if (parse_bool(text, &v)) *(bool *)field = v;
        break;
    }
    case KEY_DOUBLE: {
        char *end = NULL;
        double v = strtod(text, &end);
        if (end != text && *end == '\0' && v >= def->min && v <= def->max) *(double *)field = v;
        break;
    }
    case KEY_ENUM: {
        int v;
        if (parse_named(def, text, &v)) *(int *)field = v;
        break;
    }
    case KEY_BOOL_NAMED: {
        int v;
        if (parse_named(def, text, &v)) *(bool *)field = (v != 0);
        break;
    }
    case KEY_COLOR: {
        COLORREF v;
        if (parse_color(text, &v)) *(COLORREF *)field = v;
        break;
    }
    case KEY_MEGABYTES: {
        long v;
        if (parse_long(text, &v) && v >= def->min && v <= def->max) *(size_t *)field = (size_t)v * MIB;
        break;
    }
    }
}

static void apply_entry(Settings *s, const char *section, const char *key, const char *value)
{
    for (size_t i = 0; i < ARRAY_COUNT(KEY_TABLE); ++i) {
        const KeyDef *def = &KEY_TABLE[i];
        if (equals_ci(section, def->section) && equals_ci(key, def->key)) {
            apply_value(s, def, value);
            return;
        }
    }
    /* 不明なキーは無視する */
}

/* ------------------------------------------------------------------------
 * 読み込み
 * ------------------------------------------------------------------------ */

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
    return text;
}

static char *read_small_file(const wchar_t *path, DWORD *outSize)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;

    char *buffer = NULL;
    LARGE_INTEGER size;
    if (GetFileSizeEx(file, &size) && size.QuadPart <= MAX_INI_BYTES) {
        buffer = calloc((size_t)size.QuadPart + 1, 1);
        DWORD read = 0;
        if (buffer && ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL)) {
            buffer[read] = '\0';
            *outSize = read;
        } else {
            free(buffer);
            buffer = NULL;
        }
    }
    CloseHandle(file);
    return buffer;
}

static void parse_ini(Settings *s, char *text)
{
    static const char UTF8_BOM[] = "\xEF\xBB\xBF";
    if (strncmp(text, UTF8_BOM, 3) == 0) text += 3;

    char section[64] = "";
    char *line = text;
    while (line && *line) {
        char *next = strpbrk(line, "\r\n");
        if (next) {
            *next++ = '\0';
            next += strspn(next, "\r\n");
        }

        char *p = trim(line);
        if (*p == '[') {
            char *close = strchr(p + 1, ']');
            if (close) {
                *close = '\0';
                snprintf(section, sizeof(section), "%s", trim(p + 1));
            }
        } else if (*p != '\0' && *p != ';' && *p != '#') {
            char *equal = strchr(p, '=');
            if (equal) {
                *equal = '\0';
                apply_entry(s, section, trim(p), trim(equal + 1));
            }
        }
        line = next;
    }
}

void settings_load(Settings *s, const wchar_t *path)
{
    settings_set_defaults(s);
    if (!path) return;

    DWORD size = 0;
    char *text = read_small_file(path, &size);
    if (!text) return;
    parse_ini(s, text);
    free(text);
}

/* ------------------------------------------------------------------------
 * 保存
 * ------------------------------------------------------------------------ */

static const char *name_of(const KeyDef *def, int value)
{
    for (size_t i = 0; i < def->nameCount; ++i) {
        if (def->names[i].value == value) return def->names[i].name;
    }
    return def->nameCount ? def->names[0].name : "";
}

static int format_value(const Settings *s, const KeyDef *def, char *out, size_t capacity)
{
    const void *field = (const char *)s + def->offset;

    switch (def->type) {
    case KEY_INT:
        return snprintf(out, capacity, "%d", *(const int *)field);
    case KEY_BOOL:
        return snprintf(out, capacity, "%d", *(const bool *)field ? 1 : 0);
    case KEY_DOUBLE:
        return snprintf(out, capacity, "%.3f", *(const double *)field);
    case KEY_ENUM:
        return snprintf(out, capacity, "%s", name_of(def, *(const int *)field));
    case KEY_BOOL_NAMED:
        return snprintf(out, capacity, "%s", name_of(def, *(const bool *)field ? 1 : 0));
    case KEY_COLOR: {
        COLORREF c = *(const COLORREF *)field;
        return snprintf(out, capacity, "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    }
    case KEY_MEGABYTES:
        return snprintf(out, capacity, "%llu", (unsigned long long)(*(const size_t *)field / MIB));
    }
    return -1;
}

/* INI 本文を組み立てる。失敗時は -1。 */
static int build_ini_text(const Settings *s, char *out, size_t capacity)
{
    size_t used = 0;
    int n = snprintf(out, capacity, "[General]\r\nConfigVersion=1\r\n");
    if (n < 0 || (size_t)n >= capacity) return -1;
    used = (size_t)n;

    const char *currentSection = "";
    for (size_t i = 0; i < ARRAY_COUNT(KEY_TABLE); ++i) {
        const KeyDef *def = &KEY_TABLE[i];
        char value[64];
        if (format_value(s, def, value, sizeof(value)) < 0) return -1;

        if (strcmp(currentSection, def->section) != 0) {
            currentSection = def->section;
            n = snprintf(out + used, capacity - used, "\r\n[%s]\r\n", currentSection);
            if (n < 0 || (size_t)n >= capacity - used) return -1;
            used += (size_t)n;
        }
        n = snprintf(out + used, capacity - used, "%s=%s\r\n", def->key, value);
        if (n < 0 || (size_t)n >= capacity - used) return -1;
        used += (size_t)n;
    }
    return (int)used;
}

bool settings_save(const Settings *s, const wchar_t *path)
{
    if (!path) return false;

    char text[4096];
    int length = build_ini_text(s, text, sizeof(text));
    if (length < 0) return false;

    size_t pathLen = wcslen(path);
    wchar_t *tempPath = malloc((pathLen + 5) * sizeof(wchar_t));
    if (!tempPath) return false;
    memcpy(tempPath, path, pathLen * sizeof(wchar_t));
    memcpy(tempPath + pathLen, L".tmp", 5 * sizeof(wchar_t));

    bool ok = false;
    HANDLE file = CreateFileW(tempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        ok = WriteFile(file, text, (DWORD)length, &written, NULL)
             && written == (DWORD)length
             && FlushFileBuffers(file);
        CloseHandle(file);
        if (ok) ok = MoveFileExW(tempPath, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok) DeleteFileW(tempPath);
    }
    free(tempPath);
    return ok;
}
