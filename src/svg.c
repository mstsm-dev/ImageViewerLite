/*
 * svg.c — Direct2D による SVG のラスタライズ
 *
 * 処理の流れ
 *   1. ファイルを読み、ルート <svg> の width / height / viewBox から画素サイズを決める
 *   2. WIC ビットマップ（32bpp PBGRA）に Direct2D のレンダーターゲットを作る
 *   3. ID2D1DeviceContext5 で SVG ドキュメントを作って描画する
 *   4. 結果を DIB セクションへコピーする（以降はラスター画像と同じ扱い）
 *
 * MinGW-w64 の d2d1.h には ID2D1DeviceContext5（d2d1_3.h 相当）の C 向け定義が
 * ないため、必要な2メソッドだけを vtable の位置を指定して宣言している。
 */
#include "svg.h"

#include <shlwapi.h> /* d2d1.h より先に読み込む必要がある */
#include <d2d1.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SVG_MAX_FILE_BYTES (64u * 1024 * 1024)
#define SVG_DEFAULT_WIDTH 300.0  /* サイズ指定がない SVG の既定（ブラウザと同じ） */
#define SVG_DEFAULT_HEIGHT 150.0
#define CSS_DPI 96.0             /* SVG / CSS の 1in = 96px */

/* ------------------------------------------------------------------------
 * ID2D1DeviceContext5 の最小定義
 *
 * vtable の並び（Windows SDK の d2d1.h / d2d1_1.h / d2d1_2.h / d2d1_3.h）：
 *   IUnknown 3 → ID2D1Resource 1 → ID2D1RenderTarget 53 → ID2D1DeviceContext 35
 *   → DeviceContext1 3 → DeviceContext2 11 → DeviceContext3 2 → DeviceContext4 7
 *   → ID2D1DeviceContext5::CreateSvgDocument（先頭から0始まりで115）, DrawSvgDocument（116）
 * ------------------------------------------------------------------------ */

#define D2D1_DC5_INHERITED_SLOTS (1 + 53 + 35 + 3 + 11 + 2 + 7) /* IUnknown を除く */

typedef struct SvgDocument SvgDocument; /* ID2D1SvgDocument（IUnknown としてのみ使う） */
typedef struct SvgDeviceContext SvgDeviceContext;

typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(SvgDeviceContext *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(SvgDeviceContext *self);
    ULONG (STDMETHODCALLTYPE *Release)(SvgDeviceContext *self);
    void *inherited[D2D1_DC5_INHERITED_SLOTS];
    HRESULT (STDMETHODCALLTYPE *CreateSvgDocument)(SvgDeviceContext *self, IStream *xml,
                                                   D2D1_SIZE_F viewportSize, SvgDocument **document);
    void (STDMETHODCALLTYPE *DrawSvgDocument)(SvgDeviceContext *self, SvgDocument *document);
} SvgDeviceContextVtbl;

struct SvgDeviceContext {
    const SvgDeviceContextVtbl *lpVtbl;
};

_Static_assert(offsetof(SvgDeviceContextVtbl, CreateSvgDocument) == 115 * sizeof(void *),
               "CreateSvgDocument must be vtable slot 115");

/* IID は libuuid の版に依存しないよう自前で定義する */
static const IID IID_D2D1Factory =
    { 0x06152247, 0x6f50, 0x465a, { 0x92, 0x45, 0x11, 0x8b, 0xfd, 0x3b, 0x60, 0x07 } };
static const IID IID_D2D1DeviceContext5 =
    { 0x7836d248, 0x68cc, 0x4df6, { 0xb9, 0xe8, 0xde, 0x99, 0x1b, 0xf6, 0x2e, 0xb7 } };

/* ------------------------------------------------------------------------
 * ルート要素からのサイズ決定
 * ------------------------------------------------------------------------ */

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* 最初の <svg ...> タグの属性部分 [start, end) を探す。
 * コメント・CDATA・<!DOCTYPE> などのマークアップ宣言・処理命令（<?xml ?>）は読み飛ばす。 */
static bool find_root_tag(const char *text, const char **start, const char **end)
{
    const char *p = text;
    while ((p = strchr(p, '<')) != NULL) {
        if (strncmp(p, "<!--", 4) == 0) {
            const char *close = strstr(p + 4, "-->");
            if (!close) return false;
            p = close + 3;
            continue;
        }
        if (strncmp(p, "<![CDATA[", 9) == 0) {
            const char *close = strstr(p + 9, "]]>");
            if (!close) return false;
            p = close + 3;
            continue;
        }
        if (p[1] == '!' || p[1] == '?') { /* <!DOCTYPE ...> や <?xml ...?> */
            const char *close = strchr(p + 2, '>');
            if (!close) return false;
            p = close + 1;
            continue;
        }

        /* 最初の要素がルート。<svg> 以外（<svgfoo> など）なら SVG ではない */
        char next = p[4];
        if (strncmp(p, "<svg", 4) != 0 || (!is_space(next) && next != '>' && next != '/')) return false;

        char quote = 0;
        for (const char *q = p + 4; *q; ++q) {
            if (quote) {
                if (*q == quote) quote = 0;
            } else if (*q == '"' || *q == '\'') {
                quote = *q;
            } else if (*q == '>') {
                *start = p + 4;
                *end = q;
                return true;
            }
        }
        return false;
    }
    return false;
}

/* タグ内の属性値をコピーする。名前は完全一致（前が空白であること）。 */
static bool get_attribute(const char *start, const char *end, const char *name, char *value, size_t capacity)
{
    size_t nameLen = strlen(name);
    for (const char *p = start; p + nameLen < end; ++p) {
        if (!is_space(p[-1]) || strncmp(p, name, nameLen) != 0) continue;

        const char *q = p + nameLen;
        while (q < end && is_space(*q)) ++q;
        if (q >= end || *q != '=') continue;
        ++q;
        while (q < end && is_space(*q)) ++q;
        if (q >= end || (*q != '"' && *q != '\'')) continue;

        char quote = *q++;
        const char *close = memchr(q, quote, (size_t)(end - q));
        if (!close || (size_t)(close - q) >= capacity) return false;
        memcpy(value, q, (size_t)(close - q));
        value[close - q] = '\0';
        return true;
    }
    return false;
}

/* 長さを px に換算する。%・em・ex などは解釈できないので false。 */
static bool parse_length(const char *text, double *px)
{
    static const struct { const char *unit; double scale; } UNITS[] = {
        { "",   1.0 },
        { "px", 1.0 },
        { "pt", CSS_DPI / 72.0 },
        { "pc", CSS_DPI / 6.0 },
        { "mm", CSS_DPI / 25.4 },
        { "cm", CSS_DPI / 2.54 },
        { "in", CSS_DPI },
    };

    char *rest = NULL;
    double value = strtod(text, &rest);
    if (rest == text || !(value > 0)) return false;
    while (is_space(*rest)) ++rest;

    for (size_t i = 0; i < ARRAY_COUNT(UNITS); ++i) {
        size_t len = strlen(UNITS[i].unit);
        if (strncmp(rest, UNITS[i].unit, len) == 0) {
            const char *tail = rest + len;
            while (is_space(*tail)) ++tail;
            if (*tail == '\0') {
                *px = value * UNITS[i].scale;
                return true;
            }
        }
    }
    return false;
}

/* viewBox="minX minY width height" の幅と高さ。 */
static bool parse_viewbox(const char *text, double *width, double *height)
{
    double values[4];
    const char *p = text;
    for (int i = 0; i < 4; ++i) {
        while (is_space(*p) || *p == ',') ++p;
        char *next = NULL;
        values[i] = strtod(p, &next);
        if (next == p) return false;
        p = next;
    }
    if (!(values[2] > 0) || !(values[3] > 0)) return false;
    *width = values[2];
    *height = values[3];
    return true;
}

/* SVG の原寸（CSS px）を決める。 */
static void intrinsic_size(const char *text, double *width, double *height)
{
    double w = 0, h = 0, vbW = 0, vbH = 0;
    bool hasW = false, hasH = false, hasViewBox = false;
    const char *start, *end;
    char value[256];

    if (find_root_tag(text, &start, &end)) {
        hasW = get_attribute(start, end, "width", value, sizeof(value)) && parse_length(value, &w);
        hasH = get_attribute(start, end, "height", value, sizeof(value)) && parse_length(value, &h);
        hasViewBox = get_attribute(start, end, "viewBox", value, sizeof(value))
                     && parse_viewbox(value, &vbW, &vbH);
    }

    if (!(hasW && hasH) && hasViewBox) {
        if (hasW) h = w * vbH / vbW;       /* 片方だけ指定：縦横比は viewBox に従う */
        else if (hasH) w = h * vbW / vbH;
        else { w = vbW; h = vbH; }
        hasW = hasH = true;
    }
    *width = hasW ? w : SVG_DEFAULT_WIDTH;
    *height = hasH ? h : SVG_DEFAULT_HEIGHT;
}

/* ------------------------------------------------------------------------
 * 描画
 * ------------------------------------------------------------------------ */

static char *read_file(const wchar_t *path, DWORD *size)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;

    char *buffer = NULL;
    LARGE_INTEGER length;
    if (GetFileSizeEx(file, &length) && length.QuadPart > 0 && length.QuadPart <= SVG_MAX_FILE_BYTES) {
        buffer = malloc((size_t)length.QuadPart + 1);
        DWORD read = 0;
        if (buffer && ReadFile(file, buffer, (DWORD)length.QuadPart, &read, NULL)) {
            buffer[read] = '\0';
            *size = read;
        } else {
            free(buffer);
            buffer = NULL;
        }
    }
    CloseHandle(file);
    return buffer;
}

static HRESULT ensure_factory(DecoderContext *context)
{
    if (context->d2d) return S_OK;
    /* ワーカースレッド専用なのでシングルスレッドファクトリでよい */
    return D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_D2D1Factory, NULL,
                             (void **)&context->d2d);
}

/* bitmap に SVG を描画する。viewport は SVG の原寸、scale は原寸からの縮小率。 */
static HRESULT render_to_bitmap(ID2D1Factory *factory, IWICBitmap *bitmap, IStream *xml,
                                D2D1_SIZE_F viewport, float scale)
{
    ID2D1RenderTarget *target = NULL;
    SvgDeviceContext *dc = NULL;
    SvgDocument *document = NULL;
    D2D1_MATRIX_3X2_F transform = { .m11 = scale, .m22 = scale };
    D2D1_COLOR_F transparent = { 0.0f, 0.0f, 0.0f, 0.0f };

    D2D1_RENDER_TARGET_PROPERTIES props = {
        .type = D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
        .dpiX = 96.0f, /* 1 SVG 単位 = 1 画素 */
        .dpiY = 96.0f,
        .usage = D2D1_RENDER_TARGET_USAGE_NONE,
        .minLevel = D2D1_FEATURE_LEVEL_DEFAULT,
    };
    HRESULT hr = ID2D1Factory_CreateWicBitmapRenderTarget(factory, bitmap, &props, &target);
    if (FAILED(hr)) goto done;

    /* Windows 10 1703 より前は ID2D1DeviceContext5 がなく、ここで失敗する */
    hr = ID2D1RenderTarget_QueryInterface(target, &IID_D2D1DeviceContext5, (void **)&dc);
    if (FAILED(hr)) goto done;

    hr = dc->lpVtbl->CreateSvgDocument(dc, xml, viewport, &document);
    if (FAILED(hr)) goto done;

    ID2D1RenderTarget_BeginDraw(target);
    ID2D1RenderTarget_Clear(target, &transparent);
    ID2D1RenderTarget_SetTransform(target, &transform);
    dc->lpVtbl->DrawSvgDocument(dc, document);
    hr = ID2D1RenderTarget_EndDraw(target, NULL, NULL);

done:
    if (document) IUnknown_Release((IUnknown *)document);
    if (dc) dc->lpVtbl->Release(dc);
    if (target) ID2D1RenderTarget_Release(target);
    return hr;
}

HRESULT svg_decode(DecoderContext *context, const wchar_t *path, DecodedImage *out)
{
    memset(out, 0, sizeof(*out));
    IStream *xml = NULL;
    IWICBitmap *bitmap = NULL;
    HRESULT hr = S_OK;
    D2D1_SIZE_F viewport;

    DWORD size = 0;
    char *text = read_file(path, &size);
    if (!text) return HRESULT_FROM_WIN32(ERROR_READ_FAULT);

    /* 原寸を決め、大きすぎる場合は縦横比を保って縮小する */
    double width, height;
    intrinsic_size(text, &width, &height);
    double scale = 1.0;
    if (width > SVG_MAX_SIDE || height > SVG_MAX_SIDE) {
        scale = SVG_MAX_SIDE / (width > height ? width : height);
    }
    int pixelW = (int)ceil(width * scale);
    int pixelH = (int)ceil(height * scale);
    if (pixelW < 1) pixelW = 1;
    if (pixelH < 1) pixelH = 1;

    xml = SHCreateMemStream((const BYTE *)text, (UINT)size); /* 内容は複製される */
    free(text);
    if (!xml) return E_OUTOFMEMORY;

    hr = ensure_factory(context);
    if (FAILED(hr)) goto done;
    hr = IWICImagingFactory_CreateBitmap(context->wic, (UINT)pixelW, (UINT)pixelH,
                                         &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap);
    if (FAILED(hr)) goto done;

    viewport.width = (float)width;
    viewport.height = (float)height;
    hr = render_to_bitmap(context->d2d, bitmap, xml, viewport, (float)scale);
    if (FAILED(hr)) goto done;

    hr = decoded_image_create(pixelW, pixelH, out);
    if (FAILED(hr)) goto done;
    hr = IWICBitmap_CopyPixels(bitmap, NULL, (UINT)pixelW * 4, (UINT)out->bytes, out->bits);
    if (FAILED(hr)) goto done;
    decoded_image_finish(out);

done:
    if (FAILED(hr)) decoded_image_free(out);
    if (bitmap) IWICBitmap_Release(bitmap);
    IStream_Release(xml);
    return hr;
}

void svg_release_factory(DecoderContext *context)
{
    if (context->d2d) {
        ID2D1Factory_Release(context->d2d);
        context->d2d = NULL;
    }
}
