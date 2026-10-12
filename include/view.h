/*
 * view.h — 表示倍率・パン位置の計算（仕様書 2.1）
 *
 * ウィンドウや GDI に依存しない純粋な計算だけを置く。
 * 座標系：表示領域の左上を (0,0) とするピクセル座標。
 * パン量 (panX, panY) は「中央配置からのずれ」を表す。
 */
#ifndef IVL_VIEW_H
#define IVL_VIEW_H

#include "common.h"
#include "types.h"

#define VIEW_ZOOM_MIN 0.01 /* 手動ズームの下限 1% */
#define VIEW_ZOOM_MAX 32.0 /* 手動ズームの上限 3200% */

typedef struct {
    double zoom;
    double panX, panY;
    FitMode fit; /* 現在の倍率の決め方（手動ズーム後は FIT_MANUAL） */
} ViewState;

typedef struct {
    int width, height;
} Size;

/* 倍率を mode で計算し直し、パンを中央に戻す。FIT_MANUAL なら倍率は変えない。 */
void view_apply_fit(ViewState *view, FitMode mode, Size image, Size viewport);

/* (anchorX, anchorY) 直下の画像位置を保ったまま factor 倍する。 */
void view_zoom_at(ViewState *view, double factor, int anchorX, int anchorY, Size image, Size viewport);

/* 空白が極力見えない範囲にパンを制限する。画像が小さい軸は中央に固定。 */
void view_clamp_pan(ViewState *view, Size image, Size viewport);

/* 表示先の矩形（表示領域座標）。 */
RECT view_image_rect(const ViewState *view, Size image, Size viewport);

#endif /* IVL_VIEW_H */
