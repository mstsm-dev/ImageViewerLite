/*
 * clipboard.h — 表示中画像のクリップボードコピー（CF_DIB）
 */
#ifndef IVL_CLIPBOARD_H
#define IVL_CLIPBOARD_H

#include "common.h"
#include "decoder.h"

/*
 * デコード済み画像を原寸のまま 24bpp ボトムアップ DIB でコピーする。
 * 透過画素は background に合成する（アルファは保持しない）。
 * 失敗時は既存のクリップボード内容を変更しない。
 */
bool clipboard_copy_image(HWND owner, const DecodedImage *image, COLORREF background);

#endif /* IVL_CLIPBOARD_H */
