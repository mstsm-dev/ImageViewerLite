/*
 * window.c — メインウィンドウ（メッセージ処理・マウス入力・表示モード・描画）
 *
 * キーボード操作は resource.rc のアクセラレータ経由で WM_COMMAND として届き、
 * commands.c で処理する。ここではマウスとウィンドウ状態だけを扱う。
 */
#include "app.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <windowsx.h>

static const wchar_t WINDOW_CLASS[] = L"ImageViewerLiteWindow";

/* ------------------------------------------------------------------------
 * レイアウト
 * ------------------------------------------------------------------------ */

static bool status_bar_visible(const App *a)
{
    return a->mode == VIEW_NORMAL && a->cfg.showStatusBar;
}

Size window_viewport(const App *a)
{
    RECT client;
    GetClientRect(a->hwnd, &client);
    Size size = { client.right - client.left, client.bottom - client.top };

    if (a->statusBar && status_bar_visible(a)) {
        RECT bar;
        GetWindowRect(a->statusBar, &bar);
        size.height -= bar.bottom - bar.top;
    }
    if (size.width < 0) size.width = 0;
    if (size.height < 0) size.height = 0;
    return size;
}

/* サイズや表示モードが変わったとき：ステータスバーを合わせ、必要なら再フィット */
static void on_layout_changed(App *a)
{
    if (a->statusBar) {
        ShowWindow(a->statusBar, status_bar_visible(a) ? SW_SHOW : SW_HIDE);
        SendMessageW(a->statusBar, WM_SIZE, 0, 0);
    }
    if (!a->cfg.zoomLock && a->view.fit != FIT_MANUAL) app_fit(a, a->view.fit);
    InvalidateRect(a->hwnd, NULL, FALSE);
}

/* ------------------------------------------------------------------------
 * 表示モード（通常・シンプル・全画面）
 * ------------------------------------------------------------------------ */

static void apply_window_chrome(App *a)
{
    SetMenu(a->hwnd, a->mode == VIEW_NORMAL ? a->mainMenu : NULL);
}

static void enter_fullscreen(App *a)
{
    a->modeBeforeFullscreen = a->mode;
    a->placementBeforeFullscreen.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(a->hwnd, &a->placementBeforeFullscreen);
    a->styleBeforeFullscreen = GetWindowLongPtrW(a->hwnd, GWL_STYLE);

    LONG_PTR frameBits = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLongPtrW(a->hwnd, GWL_STYLE, (a->styleBeforeFullscreen & ~frameBits) | WS_POPUP);
    a->mode = VIEW_FULLSCREEN;
    apply_window_chrome(a);

    /* ウィンドウが最も重なるモニタ全体に広げる（解像度は変更しない） */
    MONITORINFO monitor = { .cbSize = sizeof(monitor) };
    GetMonitorInfoW(MonitorFromWindow(a->hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    const RECT *r = &monitor.rcMonitor;
    SetWindowPos(a->hwnd, HWND_TOP, r->left, r->top, r->right - r->left, r->bottom - r->top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

static void leave_fullscreen(App *a, WindowMode to)
{
    SetWindowLongPtrW(a->hwnd, GWL_STYLE, a->styleBeforeFullscreen);
    a->mode = to;
    apply_window_chrome(a);
    SetWindowPlacement(a->hwnd, &a->placementBeforeFullscreen);
    SetWindowPos(a->hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void window_set_mode(App *a, WindowMode to)
{
    if (to == a->mode) return;

    if (to == VIEW_FULLSCREEN) {
        enter_fullscreen(a);
    } else if (a->mode == VIEW_FULLSCREEN) {
        leave_fullscreen(a, to);
    } else {
        a->mode = to;
        apply_window_chrome(a);
    }
    on_layout_changed(a);
}

/* ------------------------------------------------------------------------
 * ウィンドウ位置の保存・復元
 * ------------------------------------------------------------------------ */

void window_restore_placement(App *a, int showCommand)
{
    const Settings *s = &a->cfg;
    if (s->x != CW_USEDEFAULT && s->y != CW_USEDEFAULT) {
        /* 保存値は GetWindowPlacement のワークエリア座標なので同じ API で戻す */
        WINDOWPLACEMENT placement = { .length = sizeof(placement) };
        GetWindowPlacement(a->hwnd, &placement);
        placement.flags = 0;
        placement.showCmd = SW_HIDE;
        placement.rcNormalPosition = (RECT){ s->x, s->y, s->x + s->width, s->y + s->height };
        SetWindowPlacement(a->hwnd, &placement);
    }
    ShowWindow(a->hwnd, s->maximized ? SW_SHOWMAXIMIZED : showCommand);
    UpdateWindow(a->hwnd);

    if (s->startMode == VIEW_SIMPLE) window_set_mode(a, VIEW_SIMPLE);
}

void window_save_placement(App *a)
{
    WINDOWPLACEMENT placement = { .length = sizeof(placement) };
    if (a->mode == VIEW_FULLSCREEN) placement = a->placementBeforeFullscreen;
    else if (!GetWindowPlacement(a->hwnd, &placement)) return;

    const RECT *r = &placement.rcNormalPosition;
    a->cfg.x = r->left;
    a->cfg.y = r->top;
    a->cfg.width = r->right - r->left;
    a->cfg.height = r->bottom - r->top;
    a->cfg.maximized = placement.showCmd == SW_SHOWMAXIMIZED
                       || (placement.showCmd == SW_SHOWMINIMIZED
                           && (placement.flags & WPF_RESTORETOMAXIMIZED));
    /* 全画面は保存せず、全画面に入る前のモードで次回起動する */
    a->cfg.startMode = a->mode == VIEW_FULLSCREEN ? a->modeBeforeFullscreen : a->mode;
}

/* ------------------------------------------------------------------------
 * 描画
 * ------------------------------------------------------------------------ */

static UINT placeholder_text_id(const App *a)
{
    if (a->loading) return IDS_PLACEHOLDER_LOADING;
    if (a->loadFailed) return IDS_PLACEHOLDER_ERROR;
    return IDS_PLACEHOLDER_EMPTY;
}

static void on_paint(App *a)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(a->hwnd, &ps);

    Size viewport = window_viewport(a);
    wchar_t placeholder[256];
    RenderScene scene = {
        .background = a->cfg.background,
        .checkerboard = a->cfg.checkerboard,
        .highQuality = a->cfg.highQuality,
    };

    const DecodedImage *image = app_current_image(a);
    if (image) {
        Size size = { image->width, image->height };
        view_clamp_pan(&a->view, size, viewport); /* リサイズ後も空白が出ないように */
        scene.image = image;
        scene.imageRect = view_image_rect(&a->view, size, viewport);
    } else {
        scene.placeholder = res_string(placeholder_text_id(a), placeholder, ARRAY_COUNT(placeholder));
    }

    renderer_paint(&a->renderer, dc, viewport.width, viewport.height, &scene);
    EndPaint(a->hwnd, &ps);
}

/* ------------------------------------------------------------------------
 * マウス・ドロップ
 * ------------------------------------------------------------------------ */

/* ホイール：120 単位で1ステップ。Ctrl 併用でカーソル位置中心のズーム。 */
static void on_mouse_wheel(App *a, WPARAM wParam, LPARAM lParam)
{
    bool zoom = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;
    POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; /* スクリーン座標 */
    ScreenToClient(a->hwnd, &cursor);

    a->wheelAccumulator += GET_WHEEL_DELTA_WPARAM(wParam);
    while (a->wheelAccumulator >= WHEEL_DELTA) { /* 奥へ回す：前の画像 / 拡大 */
        a->wheelAccumulator -= WHEEL_DELTA;
        if (zoom) app_zoom(a, a->cfg.zoomStep, cursor.x, cursor.y);
        else app_navigate(a, -1);
    }
    while (a->wheelAccumulator <= -WHEEL_DELTA) { /* 手前へ回す：次の画像 / 縮小 */
        a->wheelAccumulator += WHEEL_DELTA;
        if (zoom) app_zoom(a, 1.0 / a->cfg.zoomStep, cursor.x, cursor.y);
        else app_navigate(a, +1);
    }
}

static void on_drop_files(App *a, HDROP drop)
{
    /* 複数ドロップされても先頭だけを扱う */
    UINT length = DragQueryFileW(drop, 0, NULL, 0);
    wchar_t *path = length ? malloc(((size_t)length + 1) * sizeof(wchar_t)) : NULL;
    if (path && DragQueryFileW(drop, 0, path, length + 1)) app_open(a, path);
    free(path);
    DragFinish(drop);
}

static void on_context_menu(App *a, LPARAM lParam)
{
    POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (p.x == -1 && p.y == -1) { /* キーボード（Shift+F10 等）から */
        RECT r;
        GetWindowRect(a->hwnd, &r);
        p.x = r.left + 40;
        p.y = r.top + 40;
    }
    TrackPopupMenu(a->popupMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, a->hwnd, NULL);
}

/* ------------------------------------------------------------------------
 * ウィンドウプロシージャ
 * ------------------------------------------------------------------------ */

static void on_create(App *a, HWND hwnd)
{
    a->hwnd = hwnd;
    DragAcceptFiles(hwnd, TRUE);
    a->statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                   0, 0, 0, 0, hwnd, NULL, a->instance, NULL);
}

static void on_destroy(App *a)
{
    worker_stop(&a->worker);
    window_save_placement(a);
    settings_save(&a->cfg, a->iniPath);
    SetMenu(a->hwnd, NULL); /* メニューは main.c で破棄する */
    PostQuitMessage(0);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    App *a = (App *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        a = ((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)a);
    }
    if (!a) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_CREATE:
        on_create(a, hwnd);
        return 0;

    case WM_SIZE:
        on_layout_changed(a);
        return 0;

    case WM_PAINT:
        on_paint(a);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* 背景はバックバッファで描くので消去しない */

    case WM_COMMAND:
        commands_execute(a, LOWORD(wParam));
        return 0;

    case WM_INITMENUPOPUP:
        commands_update_menu(a, (HMENU)wParam);
        return 0;

    case WM_CONTEXTMENU:
        on_context_menu(a, lParam);
        return 0;

    case WM_MOUSEWHEEL:
        on_mouse_wheel(a, wParam, lParam);
        return 0;

    case WM_LBUTTONDOWN:
        a->dragging = true;
        a->lastMouse = (POINT){ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        if (a->dragging) {
            POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            app_pan(a, p.x - a->lastMouse.x, p.y - a->lastMouse.y);
            a->lastMouse = p;
        }
        return 0;

    case WM_LBUTTONUP:
        if (a->dragging) ReleaseCapture(); /* WM_CAPTURECHANGED で dragging を戻す */
        return 0;

    case WM_CAPTURECHANGED:
        a->dragging = false;
        return 0;

    case WM_LBUTTONDBLCLK:
        commands_execute(a, IDM_FULLSCREEN);
        return 0;

    case WM_DROPFILES:
        on_drop_files(a, (HDROP)wParam);
        return 0;

    case WM_APP_DECODED:
        app_on_decoded(a, (DecodeResult *)lParam);
        return 0;

    case WM_DPICHANGED: {
        const RECT *suggested = (const RECT *)lParam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DESTROY:
        on_destroy(a);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool window_register_class(HINSTANCE instance)
{
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS;
    wc.hIcon = LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                          GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    return RegisterClassExW(&wc) != 0;
}

HWND window_create(App *a)
{
    return CreateWindowExW(0, WINDOW_CLASS, APP_NAME, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CW_USEDEFAULT, CW_USEDEFAULT, a->cfg.width, a->cfg.height,
                           NULL, a->mainMenu, a->instance, a);
}
