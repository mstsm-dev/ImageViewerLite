/*
 * decoder.h — 画像デコード（仕様書 6章）
 *
 * ラスター画像：ファイル → WIC デコーダ → EXIF Orientation 補正
 *               → 32bpp PBGRA 変換 → トップダウン DIB セクション
 * SVG         ：ファイル → Direct2D SVG 描画 → 32bpp PBGRA → DIB セクション（svg.c）
 *
 * どちらも結果は DIB セクションに直接書き込むため、UI スレッド側で
 * 画素のコピーは発生しない。デコードはワーカースレッドだけで行う。
 */
#ifndef IVL_DECODER_H
#define IVL_DECODER_H

#include "common.h"

#include <wincodec.h>

/* 単一画像のデコード後バッファの安全上限（仕様目標 概ね 1GiB） */
#define DECODER_MAX_IMAGE_BYTES ((uint64_t)1024 * 1024 * 1024)

typedef struct {
    HBITMAP bitmap; /* 32bpp トップダウン DIB セクション（premultiplied BGRA） */
    void *bits;     /* bitmap の画素先頭。行の幅は width * 4 バイト */
    int width;      /* Orientation 適用後の幅 */
    int height;     /* Orientation 適用後の高さ */
    bool hasAlpha;  /* 不透明でない画素が1つでもあれば true */
    size_t bytes;   /* 画素バッファのバイト数 */
} DecodedImage;

/* ワーカースレッドで1回だけ作り、デコードのたびに使い回すファクトリ類 */
typedef struct {
    IWICImagingFactory *wic;
    struct ID2D1Factory *d2d; /* SVG 用。最初の SVG で作成（svg.c） */
} DecoderContext;

/* COM 初期化済みのスレッドで呼ぶ。 */
HRESULT decoder_context_init(DecoderContext *context);
void decoder_context_free(DecoderContext *context);

/* 先頭フレームをデコードする。失敗時は *out をゼロ初期化した状態で返す。
 * 拡張子が .svg なら SVG として描画する。 */
HRESULT decoder_decode(DecoderContext *context, const wchar_t *path, bool autoOrientation,
                       DecodedImage *out);

/* ビットマップを解放してゼロ初期化する。NULL や空でも安全。 */
void decoded_image_free(DecodedImage *image);

/* ---- デコーダ実装（decoder.c / svg.c）用の補助 ---- */

/* 上限検査のうえ width×height の DIB セクションを作る（画素は未初期化）。 */
HRESULT decoded_image_create(int width, int height, DecodedImage *out);

/* 画素を書き込んだ後に呼び、hasAlpha を判定する。 */
void decoded_image_finish(DecodedImage *image);

#endif /* IVL_DECODER_H */
