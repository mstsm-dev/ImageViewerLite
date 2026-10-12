/*
 * util.c — 文字列・パス・リソース文字列の小さな補助関数
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

wchar_t *str_dup(const wchar_t *s)
{
    if (!s) return NULL;
    size_t bytes = (wcslen(s) + 1) * sizeof(wchar_t);
    wchar_t *copy = malloc(bytes);
    if (copy) memcpy(copy, s, bytes);
    return copy;
}

wchar_t *path_join(const wchar_t *dir, const wchar_t *name)
{
    size_t dirLen = wcslen(dir);
    size_t nameLen = wcslen(name);
    bool needSlash = dirLen > 0 && dir[dirLen - 1] != L'\\';
    size_t total = dirLen + (needSlash ? 1 : 0) + nameLen + 1;

    wchar_t *joined = malloc(total * sizeof(wchar_t));
    if (!joined) return NULL;

    memcpy(joined, dir, dirLen * sizeof(wchar_t));
    if (needSlash) joined[dirLen++] = L'\\';
    memcpy(joined + dirLen, name, (nameLen + 1) * sizeof(wchar_t));
    return joined;
}

bool path_equals(const wchar_t *a, const wchar_t *b)
{
    return a && b && _wcsicmp(a, b) == 0;
}

wchar_t *module_path(void)
{
    /* 長いパスに対応するため、切り詰められなくなるまでバッファを広げる。 */
    for (DWORD capacity = MAX_PATH; capacity <= 32768; capacity *= 2) {
        wchar_t *buffer = malloc(capacity * sizeof(wchar_t));
        if (!buffer) return NULL;
        DWORD length = GetModuleFileNameW(NULL, buffer, capacity);
        if (length > 0 && length < capacity) return buffer;
        free(buffer);
        if (length == 0) return NULL;
    }
    return NULL;
}

const wchar_t *res_string(UINT id, wchar_t *buffer, int capacity)
{
    if (capacity <= 0) return buffer;
    if (LoadStringW(GetModuleHandleW(NULL), id, buffer, capacity) <= 0) buffer[0] = L'\0';
    return buffer;
}

int res_message_box(HWND owner, UINT textId, UINT flags)
{
    wchar_t text[512];
    return MessageBoxW(owner, res_string(textId, text, ARRAY_COUNT(text)), APP_NAME, flags);
}
