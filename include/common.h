/*
 * common.h — 全モジュール共通の Windows ヘッダ設定と小さなユーティリティ
 */
#ifndef IVL_COMMON_H
#define IVL_COMMON_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "version.h"
#include "resource.h"

#define APP_NAME L"" IVL_APP_NAME_STRING
#define APP_VERSION L"" IVL_VERSION_STRING

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* ワーカースレッドからUIスレッドへデコード結果を渡すメッセージ（lParam = DecodeResult*） */
#define WM_APP_DECODED (WM_APP + 1)

/* ---- 文字列・パス（util.c） ---- */

/* wcsdup 相当。s が NULL なら NULL を返す。 */
wchar_t *str_dup(const wchar_t *s);

/* dir と name を '\' で連結した新しい文字列を返す。 */
wchar_t *path_join(const wchar_t *dir, const wchar_t *name);

/* 2つのパスが同一か（大文字小文字を区別しない）。どちらかが NULL なら false。 */
bool path_equals(const wchar_t *a, const wchar_t *b);

/* 実行ファイルのフルパスを返す（長いパス対応、呼び出し側で free）。 */
wchar_t *module_path(void);

/* ---- リソース文字列（util.c） ---- */

/* STRINGTABLE の文字列を buffer に読み込む。見つからなければ空文字列。 */
const wchar_t *res_string(UINT id, wchar_t *buffer, int capacity);

/* STRINGTABLE の文字列でメッセージボックスを表示する。 */
int res_message_box(HWND owner, UINT textId, UINT flags);

#endif /* IVL_COMMON_H */
