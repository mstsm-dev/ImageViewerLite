/*
 * render.h — GDI による描画（仕様書 6章）
 *
 * メモリ DC のバックバッファに描画してから画面へ転送する（ちらつき防止）。
 * 不透明画像は StretchBlt、透過画像は AlphaBlend で背景と合成する。
 */
#ifndef IVL_RENDER_H
#define IVL_RENDER_H

#include "common.h"
#include "decoder.h"

typedef struct {
    HDC backDC;
    HBITMAP backBitmap;
    HGDIOBJ backOldBitmap;
    int backWidth, backHeight;

    HDC sourceDC;          /* 画像ビットマップを一時的に選択する DC */
    HBRUSH checkerBrush;   /* 市松模様のパターンブラシ */
} Renderer;

/* 1フレーム分の描画内容 */
typedef struct {
    const DecodedImage *image;   /* NULL なら placeholder を中央に表示 */
    RECT imageRect;              /* 表示先（表示領域座標） */
    const wchar_t *placeholder;
    COLORREF background;
    bool checkerboard;
    bool highQuality;
} RenderScene;

bool renderer_init(Renderer *renderer);
void renderer_free(Renderer *renderer);

/* target の (0,0)-(width,height) に scene を描画する。 */
void renderer_paint(Renderer *renderer, HDC target, int width, int height, const RenderScene *scene);

#endif /* IVL_RENDER_H */
