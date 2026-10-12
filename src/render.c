/*
 * render.c — GDI による描画
 *
 * GDI 資源の扱い：選択中のビットマップを削除する前に、必ず元のオブジェクトを
 * SelectObject で戻す。
 */
#include "render.h"

#include <string.h>

#define CHECKER_CELL 8
#define CHECKER_LIGHT RGB(205, 205, 205)
#define CHECKER_DARK RGB(150, 150, 150)
#define PLACEHOLDER_TEXT_COLOR RGB(205, 205, 205)

/* 2×2 マスの市松模様を1枚のパターンブラシにする（毎回の FillRect ループを避ける）。 */
static HBRUSH create_checker_brush(void)
{
    const int size = CHECKER_CELL * 2;
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = dc ? CreateCompatibleBitmap(screen, size, size) : NULL;
    ReleaseDC(NULL, screen);
    HBRUSH brush = NULL;

    if (bitmap) {
        HGDIOBJ old = SelectObject(dc, bitmap);
        HBRUSH light = CreateSolidBrush(CHECKER_LIGHT);
        HBRUSH dark = CreateSolidBrush(CHECKER_DARK);
        RECT all = { 0, 0, size, size };
        RECT topRight = { CHECKER_CELL, 0, size, CHECKER_CELL };
        RECT bottomLeft = { 0, CHECKER_CELL, CHECKER_CELL, size };
        FillRect(dc, &all, light);
        FillRect(dc, &topRight, dark);
        FillRect(dc, &bottomLeft, dark);
        DeleteObject(light);
        DeleteObject(dark);
        SelectObject(dc, old);
        brush = CreatePatternBrush(bitmap); /* ブラシはビットマップを複製して保持する */
        DeleteObject(bitmap);
    }
    if (dc) DeleteDC(dc);
    return brush;
}

bool renderer_init(Renderer *r)
{
    memset(r, 0, sizeof(*r));
    r->sourceDC = CreateCompatibleDC(NULL);
    r->checkerBrush = create_checker_brush();
    return r->sourceDC != NULL;
}

static void release_backbuffer(Renderer *r)
{
    if (r->backDC) {
        SelectObject(r->backDC, r->backOldBitmap);
        DeleteDC(r->backDC);
    }
    if (r->backBitmap) DeleteObject(r->backBitmap);
    r->backDC = NULL;
    r->backBitmap = NULL;
    r->backOldBitmap = NULL;
    r->backWidth = r->backHeight = 0;
}

void renderer_free(Renderer *r)
{
    release_backbuffer(r);
    if (r->sourceDC) DeleteDC(r->sourceDC);
    if (r->checkerBrush) DeleteObject(r->checkerBrush);
    memset(r, 0, sizeof(*r));
}

static bool ensure_backbuffer(Renderer *r, HDC target, int width, int height)
{
    if (r->backDC && r->backWidth == width && r->backHeight == height) return true;
    release_backbuffer(r);

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (!dc || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        return false;
    }
    r->backDC = dc;
    r->backBitmap = bitmap;
    r->backOldBitmap = SelectObject(dc, bitmap);
    r->backWidth = width;
    r->backHeight = height;
    return true;
}

static void fill_solid(HDC dc, const RECT *rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void draw_placeholder(HDC dc, RECT *area, const wchar_t *text)
{
    if (!text || !*text) return;
    HGDIOBJ oldFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, PLACEHOLDER_TEXT_COLOR);
    DrawTextW(dc, text, -1, area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

static void draw_checkerboard(Renderer *r, HDC dc, const RECT *imageRect, const RECT *area)
{
    RECT visible;
    if (!r->checkerBrush || !IntersectRect(&visible, imageRect, area)) return;
    /* 模様の原点を画像の左上に合わせる（パンしても模様が画像と一緒に動く） */
    SetBrushOrgEx(dc, imageRect->left, imageRect->top, NULL);
    FillRect(dc, &visible, r->checkerBrush);
}

static void draw_image(Renderer *r, HDC dc, const RenderScene *scene, const RECT *area)
{
    const DecodedImage *image = scene->image;
    const RECT *dst = &scene->imageRect;
    int dstW = dst->right - dst->left;
    int dstH = dst->bottom - dst->top;
    if (dstW <= 0 || dstH <= 0) return;

    if (image->hasAlpha && scene->checkerboard) draw_checkerboard(r, dc, dst, area);

    HGDIOBJ old = SelectObject(r->sourceDC, image->bitmap);
    if (image->hasAlpha) {
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(dc, dst->left, dst->top, dstW, dstH,
                   r->sourceDC, 0, 0, image->width, image->height, blend);
    } else {
        SetStretchBltMode(dc, scene->highQuality ? HALFTONE : COLORONCOLOR);
        if (scene->highQuality) SetBrushOrgEx(dc, 0, 0, NULL); /* HALFTONE 設定後に必要 */
        StretchBlt(dc, dst->left, dst->top, dstW, dstH,
                   r->sourceDC, 0, 0, image->width, image->height, SRCCOPY);
    }
    SelectObject(r->sourceDC, old);
}

static void draw_scene(Renderer *r, HDC dc, int width, int height, const RenderScene *scene)
{
    RECT area = { 0, 0, width, height };
    fill_solid(dc, &area, scene->background);

    if (scene->image && scene->image->bitmap) draw_image(r, dc, scene, &area);
    else draw_placeholder(dc, &area, scene->placeholder);
}

void renderer_paint(Renderer *r, HDC target, int width, int height, const RenderScene *scene)
{
    if (width <= 0 || height <= 0) return;

    if (ensure_backbuffer(r, target, width, height)) {
        draw_scene(r, r->backDC, width, height, scene);
        BitBlt(target, 0, 0, width, height, r->backDC, 0, 0, SRCCOPY);
    } else {
        draw_scene(r, target, width, height, scene); /* バックバッファが作れないときは直接描画 */
    }
}
