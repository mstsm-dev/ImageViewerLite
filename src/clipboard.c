/*
 * clipboard.c — 表示中画像のクリップボードコピー
 *
 * 画面の拡大縮小結果ではなく、デコード済み（Orientation 適用後）の原寸画像を
 * コピーする。貼り付け先との互換性を重視し、24bpp・ボトムアップ・
 * 行4バイト境界の CF_DIB を使う。
 */
#include "clipboard.h"

#include <limits.h>
#include <string.h>

/* premultiplied BGRA の1行を、背景色と合成した BGR に変換する。 */
static void convert_row(const BYTE *src, BYTE *dst, int width, COLORREF background)
{
    const unsigned bgB = GetBValue(background);
    const unsigned bgG = GetGValue(background);
    const unsigned bgR = GetRValue(background);

    for (int x = 0; x < width; ++x) {
        const BYTE *s = src + (size_t)x * 4;
        BYTE *d = dst + (size_t)x * 3;
        unsigned inverse = 255u - s[3];
        /* 前乗算済みなので「色 + 背景 × (1 - α)」で合成できる */
        d[0] = (BYTE)(s[0] + (bgB * inverse + 127) / 255);
        d[1] = (BYTE)(s[1] + (bgG * inverse + 127) / 255);
        d[2] = (BYTE)(s[2] + (bgR * inverse + 127) / 255);
    }
}

/* CF_DIB 用のメモリブロックを作る。失敗時は NULL。 */
static HGLOBAL build_dib(const DecodedImage *image, COLORREF background)
{
    uint64_t width = (uint64_t)image->width;
    uint64_t height = (uint64_t)image->height;
    uint64_t stride = (width * 3 + 3) & ~UINT64_C(3);
    uint64_t pixelBytes = stride * height;
    uint64_t total = sizeof(BITMAPINFOHEADER) + pixelBytes;
    if (pixelBytes > UINT32_MAX || total > SIZE_MAX) return NULL;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)total);
    if (!memory) return NULL;
    BYTE *base = GlobalLock(memory);
    if (!base) {
        GlobalFree(memory);
        return NULL;
    }

    BITMAPINFOHEADER *header = (BITMAPINFOHEADER *)base;
    memset(header, 0, sizeof(*header));
    header->biSize = sizeof(*header);
    header->biWidth = image->width;
    header->biHeight = image->height; /* 正の値：ボトムアップ */
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = BI_RGB;
    header->biSizeImage = (DWORD)pixelBytes;

    const BYTE *source = image->bits; /* トップダウン */
    BYTE *pixels = base + sizeof(BITMAPINFOHEADER);
    size_t srcStride = (size_t)image->width * 4;
    size_t padding = (size_t)stride - (size_t)image->width * 3;

    for (int y = 0; y < image->height; ++y) {
        const BYTE *srcRow = source + (size_t)y * srcStride;
        BYTE *dstRow = pixels + (size_t)(image->height - 1 - y) * (size_t)stride;
        convert_row(srcRow, dstRow, image->width, background);
        memset(dstRow + (size_t)image->width * 3, 0, padding);
    }

    GlobalUnlock(memory);
    return memory;
}

bool clipboard_copy_image(HWND owner, const DecodedImage *image, COLORREF background)
{
    if (!image || !image->bits || image->width <= 0 || image->height <= 0) return false;

    HGLOBAL memory = build_dib(image, background);
    if (!memory) return false;

    if (!OpenClipboard(owner)) {
        GlobalFree(memory);
        return false;
    }
    bool ok = false;
    if (EmptyClipboard() && SetClipboardData(CF_DIB, memory)) {
        memory = NULL; /* 成功時は所有権が OS に移る */
        ok = true;
    }
    CloseClipboard();
    if (memory) GlobalFree(memory);
    return ok;
}
