/*
 * main.c — ImageViewerLite のエントリーポイント
 *
 * 起動順：設定読み込み → リソース読み込み → ウィンドウ作成 → ワーカー開始
 *         → ウィンドウ表示 → コマンドライン第1引数の画像を開く → メッセージループ
 * 終了時：WM_DESTROY（window.c）でワーカー停止と設定保存、ここで残りを解放。
 *
 * DPI 対応（Per-Monitor V2）と長いパス対応はマニフェストで宣言している。
 */
#include "app.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>

static App g_app;

static bool load_ui_resources(App *a)
{
    a->mainMenu = LoadMenuW(a->instance, MAKEINTRESOURCEW(IDR_MAINMENU));
    a->popupMenuRoot = LoadMenuW(a->instance, MAKEINTRESOURCEW(IDR_POPUPMENU));
    a->popupMenu = a->popupMenuRoot ? GetSubMenu(a->popupMenuRoot, 0) : NULL;
    a->accelerators = LoadAcceleratorsW(a->instance, MAKEINTRESOURCEW(IDR_ACCELERATORS));
    return a->mainMenu && a->popupMenu && a->accelerators;
}

static void init_state(App *a, HINSTANCE instance)
{
    a->instance = instance;
    a->iniPath = settings_default_path();
    settings_load(&a->cfg, a->iniPath);

    a->cache.maxBytes = a->cfg.maxCacheBytes;
    a->view.zoom = 1.0;
    a->view.fit = a->cfg.initialFit;
    a->mode = VIEW_NORMAL;
    a->modeBeforeFullscreen = VIEW_NORMAL;
}

/* ウィンドウ破棄後に残る資源を解放する。途中で失敗した場合も呼べる。 */
static void release_state(App *a)
{
    worker_stop(&a->worker); /* 通常は WM_DESTROY で停止済み */
    cache_clear(&a->cache);
    filelist_free(&a->files);
    renderer_free(&a->renderer);
    if (a->mainMenu) DestroyMenu(a->mainMenu);
    if (a->popupMenuRoot) DestroyMenu(a->popupMenuRoot);
    /* アクセラレータテーブルはリソースから読んだものなので解放不要 */
    free(a->currentPath);
    free(a->iniPath);
}

/* コマンドライン第1引数（画像またはフォルダのパス）を返す。呼び出し側で free。 */
static wchar_t *command_line_path(void)
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    wchar_t *path = (argv && argc > 1) ? str_dup(argv[1]) : NULL;
    if (argv) LocalFree(argv);
    return path;
}

static int run_message_loop(App *a)
{
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAcceleratorW(a->hwnd, a->accelerators, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand)
{
    (void)previous;
    (void)commandLine; /* Unicode の引数は CommandLineToArgvW で取得する */

    App *a = &g_app;
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_BAR_CLASSES };
    InitCommonControlsEx(&controls);
    init_state(a, instance);

    if (!load_ui_resources(a) || !renderer_init(&a->renderer)
        || !window_register_class(instance) || !window_create(a)) {
        res_message_box(NULL, IDS_INIT_ERROR, MB_OK | MB_ICONERROR);
        release_state(a);
        return 1;
    }

    if (!worker_start(&a->worker, a->hwnd, WM_APP_DECODED)) {
        res_message_box(a->hwnd, IDS_WORKER_ERROR, MB_OK | MB_ICONERROR);
        DestroyWindow(a->hwnd);
        release_state(a);
        return 1;
    }

    window_restore_placement(a, showCommand);

    wchar_t *initialPath = command_line_path();
    if (initialPath) {
        app_open(a, initialPath);
        free(initialPath);
    }

    int exitCode = run_message_loop(a);
    release_state(a);
    return exitCode;
}
