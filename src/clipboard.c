#include "viewer.h"
#include <stdint.h>
#include <limits.h>
#include <wchar.h>
#include <string.h>

/* Copy the full decoded image, never the scaled window contents.
 * CF_DIB is a widely compatible 24-bit, bottom-up DIB; alpha pixels are
 * composited over the configured solid background color. */
BOOL app_copy_image_to_clipboard(App *a)
{
    if (!a || !a->selectedPath || a->loading) return FALSE;
    const CacheEntry *entry = NULL;
    for (int i = 0; i < CACHE_SLOTS; ++i) {
        if (a->cache[i].path &&
            _wcsicmp(a->cache[i].path, a->selectedPath) == 0) {
            entry = &a->cache[i];
            break;
        }
    }
    if (!entry || !entry->bits || entry->w <= 0 || entry->h <= 0) return FALSE;

    uint64_t width = (uint64_t)entry->w;
    uint64_t height = (uint64_t)entry->h;
    uint64_t stride = ((width * 3u + 3u) & ~UINT64_C(3));
    uint64_t byteCount = stride * height;
    uint64_t total = sizeof(BITMAPINFOHEADER) + byteCount;
    if (width > INT_MAX || height > INT_MAX || byteCount > UINT32_MAX ||
        total > (uint64_t)SIZE_MAX) return FALSE;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)total);
    if (!memory) return FALSE;
    BYTE *base = (BYTE *)GlobalLock(memory);
    if (!base) { GlobalFree(memory); return FALSE; }

    BITMAPINFOHEADER *header = (BITMAPINFOHEADER *)base;
    memset(header, 0, sizeof(*header));
    header->biSize = sizeof(*header);
    header->biWidth = entry->w;
    header->biHeight = entry->h; /* positive: clipboard DIB is bottom-up */
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = BI_RGB;
    header->biSizeImage = (DWORD)byteCount;

    /* WIC's cached source is top-down, premultiplied BGRA. */
    const BYTE *source = (const BYTE *)entry->bits;
    BYTE *target = base + sizeof(BITMAPINFOHEADER);
    const unsigned b = GetBValue(a->cfg.background);
    const unsigned g = GetGValue(a->cfg.background);
    const unsigned r = GetRValue(a->cfg.background);
    for (int y = 0; y < entry->h; ++y) {
        const BYTE *s = source + (size_t)y * (size_t)entry->w * 4;
        BYTE *d = target + (size_t)(entry->h - 1 - y) * (size_t)stride;
        for (int x = 0; x < entry->w; ++x) {
            const unsigned alpha = s[x * 4 + 3];
            const unsigned inverse = 255 - alpha;
            d[x * 3 + 0] = (BYTE)((unsigned)s[x * 4 + 0] + (b * inverse + 127) / 255);
            d[x * 3 + 1] = (BYTE)((unsigned)s[x * 4 + 1] + (g * inverse + 127) / 255);
            d[x * 3 + 2] = (BYTE)((unsigned)s[x * 4 + 2] + (r * inverse + 127) / 255);
        }
        memset(d + (size_t)entry->w * 3, 0, (size_t)stride - (size_t)entry->w * 3);
    }
    GlobalUnlock(memory);

    if (!OpenClipboard(a->hwnd)) { GlobalFree(memory); return FALSE; }
    BOOL result = FALSE;
    if (EmptyClipboard() && SetClipboardData(CF_DIB, memory)) {
        /* Ownership transfers to the clipboard on success. */
        memory = NULL;
        result = TRUE;
    }
    CloseClipboard();
    if (memory) GlobalFree(memory);
    return result;
}
