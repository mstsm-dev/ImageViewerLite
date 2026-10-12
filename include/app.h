/*
 * app.h — アプリケーション全体の状態と、モジュール間をつなぐ操作
 *
 * 担当ファイル
 *   main.c     … 起動・終了処理、メッセージループ
 *   window.c   … ウィンドウプロシージャ、マウス入力、表示モード、描画呼び出し
 *   commands.c … メニュー／アクセラレータのコマンド処理、メニューの状態表示
 *   app.c      … 画像を開く・移動・再読み込み・デコード結果の受け取り・倍率操作
 */
#ifndef IVL_APP_H
#define IVL_APP_H

#include "common.h"
#include "cache.h"
#include "filelist.h"
#include "render.h"
#include "settings.h"
#include "types.h"
#include "view.h"
#include "worker.h"

typedef struct {
    /* ---- Win32 資源 ---- */
    HINSTANCE instance;
    HWND hwnd;
    HWND statusBar;
    HMENU mainMenu;
    HMENU popupMenuRoot; /* LoadMenu の戻り値（破棄用） */
    HMENU popupMenu;     /* 実際に表示するサブメニュー */
    HACCEL accelerators;

    /* ---- 設定 ---- */
    Settings cfg;        /* 実行中の値。cfg.zoomLock がズーム固定の現在状態 */
    wchar_t *iniPath;

    /* ---- 画像 ---- */
    FileList files;
    ImageCache cache;
    DecodeWorker worker;
    wchar_t *currentPath;      /* 表示要求中の画像（キャッシュ上で固定される） */
    unsigned long generation;  /* 表示要求ごとに増やす。古いデコード結果の判別用 */
    bool loading;
    bool loadFailed;
    bool fitOnLoad;            /* 読み込み完了時に初期倍率を適用する */

    /* ---- 表示 ---- */
    Renderer renderer;
    ViewState view;
    WindowMode mode;
    WindowMode modeBeforeFullscreen;
    WINDOWPLACEMENT placementBeforeFullscreen;
    LONG_PTR styleBeforeFullscreen;

    /* ---- 入力 ---- */
    bool dragging;
    POINT lastMouse;
    int wheelAccumulator;
} App;

/* ---- app.c ---- */
void app_open(App *app, const wchar_t *path);
void app_navigate(App *app, int delta);      /* -1: 前、+1: 次（ループ設定に従う） */
void app_navigate_edge(App *app, bool last); /* false: 最初、true: 最後 */
void app_reload(App *app);                   /* 一覧の再生成とキャッシュ破棄 */
void app_change_sort(App *app, SortMode sort, bool descending);
void app_on_decoded(App *app, DecodeResult *result);
const DecodedImage *app_current_image(App *app);
void app_fit(App *app, FitMode mode);
void app_zoom(App *app, double factor, int anchorX, int anchorY);
void app_zoom_center(App *app, double factor);
void app_pan(App *app, int dx, int dy);
void app_update_ui(App *app);                /* タイトル・ステータスバー・再描画 */

/* ---- window.c ---- */
bool window_register_class(HINSTANCE instance);
HWND window_create(App *app);
void window_restore_placement(App *app, int showCommand);
void window_set_mode(App *app, WindowMode mode);
void window_save_placement(App *app);        /* 現在の位置・サイズ・モードを cfg へ */
Size window_viewport(const App *app);        /* 画像表示領域（ステータスバーを除く） */

/* ---- commands.c ---- */
void commands_execute(App *app, int commandId);
void commands_update_menu(App *app, HMENU menu);

#endif /* IVL_APP_H */
