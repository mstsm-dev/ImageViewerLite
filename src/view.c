/*
 * view.c — 表示倍率・パン位置の計算
 */
#include "view.h"

#include <math.h>

static double min2(double a, double b) { return a < b ? a : b; }

static double fit_zoom(FitMode mode, double current, Size image, Size viewport)
{
    double zx = (double)viewport.width / image.width;
    double zy = (double)viewport.height / image.height;

    switch (mode) {
    case FIT_ALL:    return min2(zx, zy);
    case FIT_SHRINK: return min2(1.0, min2(zx, zy));
    case FIT_ACTUAL: return 1.0;
    case FIT_WIDTH:  return zx;
    case FIT_HEIGHT: return zy;
    default:         return current; /* FIT_MANUAL */
    }
}

void view_apply_fit(ViewState *view, FitMode mode, Size image, Size viewport)
{
    view->fit = mode;
    view->panX = 0;
    view->panY = 0;
    if (image.width <= 0 || image.height <= 0 || viewport.width <= 0 || viewport.height <= 0) return;
    /* 巨大な画像の全体フィットは手動ズームの下限 1% を下回ってもよい */
    view->zoom = fit_zoom(mode, view->zoom, image, viewport);
}

void view_zoom_at(ViewState *view, double factor, int anchorX, int anchorY, Size image, Size viewport)
{
    double oldZoom = view->zoom;
    if (oldZoom <= 0 || image.width <= 0 || image.height <= 0) return;

    double newZoom = oldZoom * factor;
    if (newZoom < VIEW_ZOOM_MIN) newZoom = VIEW_ZOOM_MIN;
    if (newZoom > VIEW_ZOOM_MAX) newZoom = VIEW_ZOOM_MAX;
    /* フィットで範囲外になっている倍率から、意図と逆方向へ跳ばないようにする */
    if ((factor < 1.0 && newZoom >= oldZoom) || (factor > 1.0 && newZoom <= oldZoom)) return;

    /* アンカー直下の画像座標 */
    double originX = (viewport.width - image.width * oldZoom) / 2 + view->panX;
    double originY = (viewport.height - image.height * oldZoom) / 2 + view->panY;
    double imageX = (anchorX - originX) / oldZoom;
    double imageY = (anchorY - originY) / oldZoom;

    view->zoom = newZoom;
    view->fit = FIT_MANUAL;
    view->panX = anchorX - imageX * newZoom - (viewport.width - image.width * newZoom) / 2;
    view->panY = anchorY - imageY * newZoom - (viewport.height - image.height * newZoom) / 2;
    view_clamp_pan(view, image, viewport);
}

static double clamp_axis(double pan, double scaled, int available)
{
    if (scaled <= available) return 0;
    double limit = (scaled - available) / 2;
    if (pan > limit) return limit;
    if (pan < -limit) return -limit;
    return pan;
}

void view_clamp_pan(ViewState *view, Size image, Size viewport)
{
    view->panX = clamp_axis(view->panX, image.width * view->zoom, viewport.width);
    view->panY = clamp_axis(view->panY, image.height * view->zoom, viewport.height);
}

RECT view_image_rect(const ViewState *view, Size image, Size viewport)
{
    double scaledW = image.width * view->zoom;
    double scaledH = image.height * view->zoom;
    int left = (int)lround((viewport.width - scaledW) / 2 + view->panX);
    int top = (int)lround((viewport.height - scaledH) / 2 + view->panY);
    RECT rect = { left, top, left + (int)lround(scaledW), top + (int)lround(scaledH) };
    return rect;
}
