/*
 * commands.c — メニュー・アクセラレータ・右クリックメニューのコマンド処理
 */
#include "app.h"
#include "clipboard.h"
#include "dialogs.h"
#include "formats.h"

#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* フィット系コマンドと FitMode の対応（実行とメニューのチェック表示で共用） */
static const struct {
    int command;
    FitMode fit;
} FIT_COMMANDS[] = {
    { IDM_ACTUAL, FIT_ACTUAL },
    { IDM_SHRINK, FIT_SHRINK },
    { IDM_FIT,    FIT_ALL },
    { IDM_WIDTH,  FIT_WIDTH },
    { IDM_HEIGHT, FIT_HEIGHT },
};

/* 長いパスにも対応できる大きさ（仕様書 4章 7） */
#define OPEN_PATH_CAPACITY 32768

/* ------------------------------------------------------------------------
 * 個別コマンド
 * ------------------------------------------------------------------------ */

/* "ラベル\0パターン\0" を追記し、追記後の位置を返す。 */
static wchar_t *append_filter(wchar_t *cursor, const wchar_t *label, const wchar_t *pattern)
{
    size_t labelLen = wcslen(label) + 1;
    size_t patternLen = wcslen(pattern) + 1;
    memcpy(cursor, label, labelLen * sizeof(wchar_t));
    cursor += labelLen;
    memcpy(cursor, pattern, patternLen * sizeof(wchar_t));
    return cursor + patternLen;
}

static wchar_t *build_open_filter(void)
{
    wchar_t imagesLabel[128];
    wchar_t allLabel[128];
    res_string(IDS_FILTER_IMAGES, imagesLabel, ARRAY_COUNT(imagesLabel));
    res_string(IDS_FILTER_ALL, allLabel, ARRAY_COUNT(allLabel));

    wchar_t *pattern = format_build_pattern();
    if (!pattern) return NULL;

    size_t length = wcslen(imagesLabel) + wcslen(pattern) + wcslen(allLabel) + wcslen(L"*.*") + 5;
    wchar_t *filter = malloc(length * sizeof(wchar_t));
    if (filter) {
        wchar_t *cursor = append_filter(filter, imagesLabel, pattern);
        cursor = append_filter(cursor, allLabel, L"*.*");
        *cursor = L'\0'; /* フィルタ全体の終端は \0\0 */
    }
    free(pattern);
    return filter;
}

static void open_file(App *a)
{
    wchar_t *path = calloc(OPEN_PATH_CAPACITY, sizeof(wchar_t));
    wchar_t *filter = build_open_filter();
    if (path && filter) {
        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = a->hwnd;
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = path;
        ofn.nMaxFile = OPEN_PATH_CAPACITY;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) app_open(a, path);
    }
    free(filter);
    free(path);
}

static void copy_image(App *a)
{
    const DecodedImage *image = app_current_image(a);
    if (!image || !clipboard_copy_image(a->hwnd, image, a->cfg.background)) {
        res_message_box(a->hwnd, IDS_CLIPBOARD_ERROR, MB_OK | MB_ICONWARNING);
    }
}

static void show_image_info(App *a)
{
    const FileEntry *entry = filelist_current(&a->files);
    if (!entry) return; /* ファイル未選択なら開かない */

    ImageInfo info = {
        .file = entry,
        .directory = a->files.directory,
        .position = a->files.index + 1,
        .count = a->files.count,
        .image = app_current_image(a),
        .loading = a->loading,
        .zoom = a->view.zoom,
    };
    info_dialog_show(a->hwnd, &info);
}

static void show_settings(App *a)
{
    if (!settings_dialog_show(a->hwnd, &a->cfg)) return;

    app_fit(a, a->cfg.initialFit);
    if (!settings_save(&a->cfg, a->iniPath)) {
        res_message_box(a->hwnd, IDS_SAVE_ERROR, MB_OK | MB_ICONWARNING);
    }
}

static void show_about(App *a)
{
    wchar_t format[256];
    wchar_t text[512];
    res_string(IDS_ABOUT_FORMAT, format, ARRAY_COUNT(format));
    swprintf(text, ARRAY_COUNT(text), format, APP_NAME, APP_VERSION);
    MessageBoxW(a->hwnd, text, APP_NAME, MB_OK | MB_ICONINFORMATION);
}

static void toggle_fullscreen(App *a)
{
    window_set_mode(a, a->mode == VIEW_FULLSCREEN ? a->modeBeforeFullscreen : VIEW_FULLSCREEN);
}

/* Esc：全画面なら元の表示に戻り、それ以外ではアプリを終了する。 */
static void escape(App *a)
{
    if (a->mode == VIEW_FULLSCREEN) toggle_fullscreen(a);
    else DestroyWindow(a->hwnd);
}

/* ------------------------------------------------------------------------
 * 公開関数
 * ------------------------------------------------------------------------ */

void commands_execute(App *a, int id)
{
    for (size_t i = 0; i < ARRAY_COUNT(FIT_COMMANDS); ++i) {
        if (FIT_COMMANDS[i].command == id) {
            app_fit(a, FIT_COMMANDS[i].fit);
            return;
        }
    }
    if (id >= IDM_SORT_NATURAL && id < IDM_SORT_NATURAL + SORT_COUNT) {
        app_change_sort(a, (SortMode)(id - IDM_SORT_NATURAL), a->cfg.descending);
        return;
    }

    switch (id) {
    /* ファイル */
    case IDM_OPEN:      open_file(a); break;
    case IDM_RELOAD:    app_reload(a); break;
    case IDM_COPY:      copy_image(a); break;
    case IDM_IMAGEINFO: show_image_info(a); break;
    case IDM_EXIT:      DestroyWindow(a->hwnd); break;
    case IDM_ESCAPE:    escape(a); break;

    /* 移動 */
    case IDM_PREV:      app_navigate(a, -1); break;
    case IDM_NEXT:      app_navigate(a, +1); break;
    case IDM_FIRST:     app_navigate_edge(a, false); break;
    case IDM_LAST:      app_navigate_edge(a, true); break;
    case IDM_SORT_DESC: app_change_sort(a, a->cfg.sort, !a->cfg.descending); break;

    /* 表示 */
    case IDM_ZOOMIN:     app_zoom_center(a, a->cfg.zoomStep); break;
    case IDM_ZOOMOUT:    app_zoom_center(a, 1.0 / a->cfg.zoomStep); break;
    case IDM_LOCK:       a->cfg.zoomLock = !a->cfg.zoomLock; break;
    case IDM_FULLSCREEN: toggle_fullscreen(a); break;
    case IDM_SIMPLE:     window_set_mode(a, a->mode == VIEW_SIMPLE ? VIEW_NORMAL : VIEW_SIMPLE); break;

    /* 設定・ヘルプ */
    case IDM_SETTINGS:  show_settings(a); break;
    case IDM_ABOUT:     show_about(a); break;
    }
}

static void check(HMENU menu, int command, bool checked)
{
    CheckMenuItem(menu, (UINT)command, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}

/* メニューを開く直前に、現在の状態をチェックマークで示す。 */
void commands_update_menu(App *a, HMENU menu)
{
    for (size_t i = 0; i < ARRAY_COUNT(FIT_COMMANDS); ++i) {
        check(menu, FIT_COMMANDS[i].command, a->view.fit == FIT_COMMANDS[i].fit);
    }
    for (int i = 0; i < SORT_COUNT; ++i) {
        check(menu, IDM_SORT_NATURAL + i, a->cfg.sort == (SortMode)i);
    }
    check(menu, IDM_SORT_DESC, a->cfg.descending);
    check(menu, IDM_LOCK, a->cfg.zoomLock);
    check(menu, IDM_SIMPLE, a->mode == VIEW_SIMPLE);
    check(menu, IDM_FULLSCREEN, a->mode == VIEW_FULLSCREEN);
}
