/*
 * svg.h — Direct2D による SVG のラスタライズ
 *
 * Windows 10 1703 以降の Direct2D SVG 機能（ID2D1DeviceContext5）を使う。
 * 外部ライブラリは使わない。対応範囲は Direct2D の SVG サポートに従い、
 * text・<style> 要素（CSS）・filter・mask・pattern・marker・アニメーションは
 * 描画されない（その要素だけ無視され、残りは描画される）。
 *
 * 画像サイズ（＝原寸 100% の画素数）はルート <svg> の width / height、
 * なければ viewBox、どちらもなければ 300×150（ブラウザの既定値）とする。
 */
#ifndef IVL_SVG_H
#define IVL_SVG_H

#include "decoder.h"

/* 1辺の上限。超える場合は縦横比を保って縮小して描画する。 */
#define SVG_MAX_SIDE 16384

HRESULT svg_decode(DecoderContext *context, const wchar_t *path, DecodedImage *out);

/* DecoderContext が持つ Direct2D ファクトリを解放する。 */
void svg_release_factory(DecoderContext *context);

#endif /* IVL_SVG_H */
