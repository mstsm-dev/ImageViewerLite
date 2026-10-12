/*
 * app.c — 画像を開く・移動・再読み込み・デコード結果の受け取り・倍率操作
 *
 * 表示要求の流れ
 *   show_current() で世代番号を進め、キャッシュになければワーカーへ要求する。
 *   ワーカーは「現在画像 → 次 → 前」の順にデコードし、WM_APP_DECODED で返す。
 *   app_on_decoded() は世代が一致する結果だけを採用する（連続送り時は最新要求のみ）。
 */
#include "app.h"
#include "formats.h"

#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

/* ------------------------------------------------------------------------
 * 内部処理
 * ------------------------------------------------------------------------ */

static Size image_size(const DecodedImage *image)
{
    Size size = { image->width, image->height };
    return size;
}

static void invalidate(App *a)
{
    InvalidateRect(a->hwnd, NULL, FALSE);
}

static void update_status(App *a)
{
    if (!a->statusBar) return;

    wchar_t line[512];
    wchar_t state[64] = L"";
    size_t position = a->files.count ? a->files.index + 1 : 0;
    if (a->loading) res_string(IDS_STATUS_LOADING, state, ARRAY_COUNT(state));
    else if (a->loadFailed) res_string(IDS_STATUS_FAILED, state, ARRAY_COUNT(state));

    const DecodedImage *image = app_current_image(a);
    if (image) {
        const wchar_t *format = a->currentPath ? format_name(a->currentPath) : NULL;
        swprintf(line, ARRAY_COUNT(line), L"  %zu / %zu     %d × %d     %.1f%%     %ls",
                 position, a->files.count, image->width, image->height, a->view.zoom * 100.0,
                 format ? format : L"");
    } else {
        swprintf(line, ARRAY_COUNT(line), L"  %zu / %zu     %ls", position, a->files.count, state);
    }
    SendMessageW(a->statusBar, SB_SETTEXTW, 0, (LPARAM)line);
}

static void update_title(App *a)
{
    wchar_t title[1024];
    const FileEntry *entry = filelist_current(&a->files);
    if (entry) swprintf(title, ARRAY_COUNT(title), L"%.900ls - %ls", entry->name, APP_NAME);
    else swprintf(title, ARRAY_COUNT(title), L"%ls", APP_NAME);
    SetWindowTextW(a->hwnd, title);
}

/* 倍率やパンだけが変わったときの更新 */
static void refresh_view(App *a)
{
    update_status(a);
    invalidate(a);
}

/* 現在画像（未キャッシュ時）と前後1枚の先読みをワーカーへ要求する。 */
static void request_decode(App *a)
{
    DecodeJob jobs[WORKER_MAX_JOBS];
    size_t count = 0;

    if (a->currentPath && !cache_contains(&a->cache, a->currentPath)) {
        jobs[count++] = (DecodeJob){ a->currentPath, false };
    }

    size_t index = a->files.index;
    size_t next = filelist_step(&a->files, index, +1, a->cfg.loop);
    size_t prev = filelist_step(&a->files, index, -1, a->cfg.loop);
    if (next != SIZE_MAX && next != index && !cache_contains(&a->cache, a->files.items[next].path)) {
        jobs[count++] = (DecodeJob){ a->files.items[next].path, true };
    }
    if (prev != SIZE_MAX && prev != index && prev != next
        && !cache_contains(&a->cache, a->files.items[prev].path)) {
        jobs[count++] = (DecodeJob){ a->files.items[prev].path, true };
    }

    /* ジョブが0件でも送る：世代が進み、処理中の古い要求が破棄される */
    worker_submit(&a->worker, a->generation, jobs, count, a->cfg.autoOrientation);
}

/*
 * 一覧の現在位置の画像を表示対象にする。
 * keepZoom=false なら初期倍率を適用し、true なら倍率を引き継ぐ（ズーム固定）。
 * どちらの場合もパン位置は中央に戻す。
 */
static void show_current(App *a, bool keepZoom)
{
    const FileEntry *entry = filelist_current(&a->files);
    if (!entry) return;
    wchar_t *path = str_dup(entry->path);
    if (!path) return;

    free(a->currentPath);
    a->currentPath = path;
    a->generation++;
    a->loadFailed = false;
    a->view.panX = 0;
    a->view.panY = 0;
    if (!keepZoom) {
        a->view.fit = a->cfg.initialFit;
        a->view.zoom = 1.0;
    }

    bool cached = cache_get(&a->cache, path) != NULL;
    a->loading = !cached;
    a->fitOnLoad = !keepZoom && !cached;
    if (cached && !keepZoom) app_fit(a, a->view.fit);

    request_decode(a);
    app_update_ui(a);
}

static void finish_current_load(App *a, bool succeeded)
{
    a->loading = false;
    a->loadFailed = !succeeded;
    if (succeeded && a->fitOnLoad) app_fit(a, a->view.fit);
    a->fitOnLoad = false;
}

/* 一覧を作り直し、現在画像を同じ位置に保つ。 */
static FileListResult relist_current(App *a)
{
    wchar_t *path = str_dup(a->currentPath);
    if (!path) return FILELIST_FAILED;
    FileListResult result = filelist_load(&a->files, path, a->cfg.sort, a->cfg.descending);
    free(path);
    return result;
}

/* ------------------------------------------------------------------------
 * 公開関数：ファイル・移動
 * ------------------------------------------------------------------------ */

void app_open(App *a, const wchar_t *path)
{
    switch (filelist_load(&a->files, path, a->cfg.sort, a->cfg.descending)) {
    case FILELIST_OK:
        /* 別フォルダを開くときはズーム固定でも初期倍率を適用する */
        show_current(a, false);
        break;
    case FILELIST_NO_IMAGES:
        res_message_box(a->hwnd, IDS_FOLDER_EMPTY, MB_OK | MB_ICONINFORMATION);
        break;
    case FILELIST_FAILED:
        res_message_box(a->hwnd, IDS_OPEN_FAILED, MB_OK | MB_ICONWARNING);
        break;
    }
}

void app_navigate(App *a, int delta)
{
    size_t next = filelist_step(&a->files, a->files.index, delta, a->cfg.loop);
    if (next == SIZE_MAX || next == a->files.index) return;
    a->files.index = next;
    show_current(a, a->cfg.zoomLock);
}

void app_navigate_edge(App *a, bool last)
{
    if (a->files.count == 0) return;
    size_t target = last ? a->files.count - 1 : 0;
    if (target == a->files.index) return;
    a->files.index = target;
    show_current(a, a->cfg.zoomLock);
}

void app_reload(App *a)
{
    if (!a->currentPath) return;
    cache_clear(&a->cache);
    if (relist_current(a) == FILELIST_OK) {
        show_current(a, false);
    } else {
        a->loading = false;
        a->loadFailed = true;
        app_update_ui(a);
    }
}

void app_change_sort(App *a, SortMode sort, bool descending)
{
    a->cfg.sort = sort;
    a->cfg.descending = descending;
    if (!a->currentPath) return;

    /* 表示中の画像は変わらないので、キャッシュと表示状態は維持する */
    if (relist_current(a) == FILELIST_OK) {
        a->generation++;
        request_decode(a); /* 前後の画像が変わるため先読みをやり直す */
    }
    app_update_ui(a);
}

void app_on_decoded(App *a, DecodeResult *r)
{
    if (r->generation == a->generation) {
        bool isCurrent = !r->prefetch && path_equals(r->path, a->currentPath);
        if (SUCCEEDED(r->hr)) {
            bool stored = cache_put(&a->cache, r->path, &r->image, a->currentPath);
            if (isCurrent) finish_current_load(a, stored);
        } else if (isCurrent) {
            finish_current_load(a, false);
        }
        app_update_ui(a);
    }
    decode_result_free(r);
}

const DecodedImage *app_current_image(App *a)
{
    return a->currentPath ? cache_get(&a->cache, a->currentPath) : NULL;
}

/* ------------------------------------------------------------------------
 * 公開関数：倍率・パン
 * ------------------------------------------------------------------------ */

void app_fit(App *a, FitMode mode)
{
    const DecodedImage *image = app_current_image(a);
    if (image) {
        view_apply_fit(&a->view, mode, image_size(image), window_viewport(a));
    } else {
        /* 画像がまだないときはモードだけ覚え、読み込み完了時に適用する */
        a->view.fit = mode;
        a->view.zoom = 1.0;
        a->view.panX = 0;
        a->view.panY = 0;
    }
    refresh_view(a);
}

void app_zoom(App *a, double factor, int anchorX, int anchorY)
{
    const DecodedImage *image = app_current_image(a);
    if (!image) return;
    view_zoom_at(&a->view, factor, anchorX, anchorY, image_size(image), window_viewport(a));
    refresh_view(a);
}

void app_zoom_center(App *a, double factor)
{
    Size viewport = window_viewport(a);
    app_zoom(a, factor, viewport.width / 2, viewport.height / 2);
}

void app_pan(App *a, int dx, int dy)
{
    const DecodedImage *image = app_current_image(a);
    if (!image) return;
    a->view.panX += dx;
    a->view.panY += dy;
    view_clamp_pan(&a->view, image_size(image), window_viewport(a));
    invalidate(a);
}

void app_update_ui(App *a)
{
    update_title(a);
    update_status(a);
    invalidate(a);
}
