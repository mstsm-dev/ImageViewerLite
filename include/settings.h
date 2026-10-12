/*
 * settings.h — INI 設定の保持・読み込み・保存
 *
 * INI のキーと Settings のフィールドの対応は settings.c の KEY_TABLE に
 * 一覧で定義している。項目を追加するときはフィールドと表の1行を足す。
 */
#ifndef IVL_SETTINGS_H
#define IVL_SETTINGS_H

#include "common.h"
#include "types.h"

#define SETTINGS_FILE_NAME L"ImageViewerLite.ini"

typedef struct {
    /* [Window] */
    int x, y;            /* 通常時の位置（CW_USEDEFAULT なら OS 任せ） */
    int width, height;   /* 通常時のサイズ */
    bool maximized;
    WindowMode startMode; /* VIEW_NORMAL / VIEW_SIMPLE のみ保存 */
    COLORREF background;
    bool showStatusBar;

    /* [Display] */
    FitMode initialFit;
    double zoomStep;
    bool zoomLock;
    bool highQuality;     /* true: HALFTONE、false: COLORONCOLOR */
    bool checkerboard;    /* 透過部分を市松模様で表示 */
    bool autoOrientation; /* EXIF Orientation を適用 */

    /* [Navigation] */
    SortMode sort;
    bool descending;
    bool loop;

    /* [Cache] */
    size_t maxCacheBytes;
} Settings;

void settings_set_defaults(Settings *s);

/* EXE と同じフォルダの INI パスを返す（呼び出し側で free）。 */
wchar_t *settings_default_path(void);

/* 既定値を設定したうえで INI を読み込む。ファイルがなければ既定値のまま。 */
void settings_load(Settings *s, const wchar_t *path);

/* INI を UTF-8（BOM なし）で一時ファイル経由で保存する。 */
bool settings_save(const Settings *s, const wchar_t *path);

#endif /* IVL_SETTINGS_H */
