/*
 * filelist.c — フォルダ直下の画像列挙と並べ替え
 *
 * サブフォルダは再帰走査しない。並べ替えは同値のとき名前をタイブレークに使う。
 */
#include "filelist.h"
#include "formats.h"

#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ------------------------------------------------------------------------
 * 列挙
 * ------------------------------------------------------------------------ */

static bool list_push(FileList *list, const wchar_t *directory, const WIN32_FIND_DATAW *data)
{
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 64;
        if (capacity > SIZE_MAX / sizeof(FileEntry)) return false;
        FileEntry *items = realloc(list->items, capacity * sizeof(FileEntry));
        if (!items) return false;
        list->items = items;
        list->capacity = capacity;
    }

    FileEntry entry = { 0 };
    entry.path = path_join(directory, data->cFileName);
    entry.name = str_dup(data->cFileName);
    if (!entry.path || !entry.name) {
        free(entry.path);
        free(entry.name);
        return false;
    }
    entry.size = ((ULONGLONG)data->nFileSizeHigh << 32) | data->nFileSizeLow;
    entry.modified = data->ftLastWriteTime;
    list->items[list->count++] = entry;
    return true;
}

static bool enumerate_directory(FileList *list, const wchar_t *directory)
{
    wchar_t *pattern = path_join(directory, L"*");
    if (!pattern) return false;

    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileExW(pattern, FindExInfoBasic, &data, FindExSearchNameMatch, NULL,
                                   FIND_FIRST_EX_LARGE_FETCH);
    free(pattern);
    if (find == INVALID_HANDLE_VALUE) return false;

    bool ok = true;
    do {
        bool isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!isDirectory && format_is_supported(data.cFileName)) {
            if (!list_push(list, directory, &data)) {
                ok = false;
                break;
            }
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return ok;
}

/* ------------------------------------------------------------------------
 * 並べ替え
 *
 * qsort の比較関数に文脈を渡せないため、静的変数で並べ替え条件を渡す。
 * 一覧の生成は UI スレッドだけで行うので競合しない。
 * ------------------------------------------------------------------------ */

static SortMode s_sortMode;
static bool s_sortDescending;

static int compare_names_ordinal(const wchar_t *a, const wchar_t *b, bool ignoreCase)
{
    return CompareStringOrdinal(a, -1, b, -1, ignoreCase) - CSTR_EQUAL;
}

static int compare_entries(const void *pa, const void *pb)
{
    const FileEntry *a = pa;
    const FileEntry *b = pb;
    int result = 0;

    switch (s_sortMode) {
    case SORT_NATURAL: result = StrCmpLogicalW(a->name, b->name); break;
    case SORT_NAME:    result = compare_names_ordinal(a->name, b->name, true); break;
    case SORT_DATE:    result = CompareFileTime(&a->modified, &b->modified); break;
    case SORT_SIZE:    result = (a->size > b->size) - (a->size < b->size); break;
    default:           break;
    }
    if (result == 0) result = compare_names_ordinal(a->name, b->name, false);
    if (result == 0) result = wcscmp(a->path, b->path);
    return s_sortDescending ? -result : result;
}

static void sort_list(FileList *list, SortMode sort, bool descending)
{
    s_sortMode = sort;
    s_sortDescending = descending;
    if (list->count > 1) qsort(list->items, list->count, sizeof(FileEntry), compare_entries);
}

/* ------------------------------------------------------------------------
 * パス処理
 * ------------------------------------------------------------------------ */

static wchar_t *full_path(const wchar_t *path)
{
    DWORD needed = GetFullPathNameW(path, 0, NULL, NULL);
    if (needed == 0) return NULL;
    wchar_t *buffer = malloc((size_t)needed * sizeof(wchar_t));
    if (!buffer) return NULL;
    DWORD length = GetFullPathNameW(path, needed, buffer, NULL);
    if (length == 0 || length >= needed) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

/* フルパスから親フォルダを取り出す。"C:\a.jpg" は "C:\" にする。 */
static wchar_t *parent_directory(const wchar_t *fullPath)
{
    const wchar_t *lastSlash = wcsrchr(fullPath, L'\\');
    if (!lastSlash) return NULL;

    size_t length = (size_t)(lastSlash - fullPath);
    if (length == 0 || (length == 2 && fullPath[1] == L':')) length++; /* ルートは '\' を残す */

    wchar_t *dir = malloc((length + 1) * sizeof(wchar_t));
    if (!dir) return NULL;
    memcpy(dir, fullPath, length * sizeof(wchar_t));
    dir[length] = L'\0';
    return dir;
}

/* ------------------------------------------------------------------------
 * 公開関数
 * ------------------------------------------------------------------------ */

FileListResult filelist_load(FileList *list, const wchar_t *path, SortMode sort, bool descending)
{
    FileList loaded = { 0 };
    FileListResult result = FILELIST_FAILED;
    wchar_t *fullPath = full_path(path);
    if (!fullPath) return FILELIST_FAILED;

    DWORD attributes = GetFileAttributesW(fullPath);
    bool isDirectory = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);

    loaded.directory = isDirectory ? str_dup(fullPath) : parent_directory(fullPath);
    if (!loaded.directory || !enumerate_directory(&loaded, loaded.directory)) goto done;
    sort_list(&loaded, sort, descending);

    if (isDirectory) {
        if (loaded.count == 0) {
            result = FILELIST_NO_IMAGES;
            goto done;
        }
        loaded.index = 0;
    } else {
        loaded.index = SIZE_MAX;
        for (size_t i = 0; i < loaded.count; ++i) {
            if (path_equals(loaded.items[i].path, fullPath)) {
                loaded.index = i;
                break;
            }
        }
        if (loaded.index == SIZE_MAX) goto done; /* 非対応拡張子・存在しないファイル */
    }

    filelist_free(list);
    *list = loaded;
    memset(&loaded, 0, sizeof(loaded));
    result = FILELIST_OK;

done:
    filelist_free(&loaded);
    free(fullPath);
    return result;
}

void filelist_free(FileList *list)
{
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i].path);
        free(list->items[i].name);
    }
    free(list->items);
    free(list->directory);
    memset(list, 0, sizeof(*list));
}

const FileEntry *filelist_current(const FileList *list)
{
    return (list->count > 0 && list->index < list->count) ? &list->items[list->index] : NULL;
}

size_t filelist_step(const FileList *list, size_t index, int delta, bool loop)
{
    size_t n = list->count;
    if (n == 0 || index >= n) return SIZE_MAX;

    if (delta > 0) {
        if (index + 1 < n) return index + 1;
        return loop ? 0 : SIZE_MAX;
    }
    if (delta < 0) {
        if (index > 0) return index - 1;
        return loop ? n - 1 : SIZE_MAX;
    }
    return index;
}
