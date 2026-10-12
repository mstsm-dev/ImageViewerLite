/*
 * formats.c — 対応拡張子と形式名
 *
 * 拡張子にかかわらず、WIC が実際にデコードできなければ表示エラーになる。
 * WebP / HEIF / AVIF は OS にインストールされた WIC コーデックに依存する。
 * SVG は Direct2D の SVG 機能で描画する（Windows 10 1703 以降）。
 */
#include "formats.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
    const wchar_t *extension; /* 先頭のドットを含む */
    const wchar_t *name;
    FormatKind kind;
} ImageFormat;

static const ImageFormat FORMATS[] = {
    { L".jpg",  L"JPEG", FORMAT_KIND_RASTER },
    { L".jpeg", L"JPEG", FORMAT_KIND_RASTER },
    { L".jpe",  L"JPEG", FORMAT_KIND_RASTER },
    { L".png",  L"PNG",  FORMAT_KIND_RASTER },
    { L".bmp",  L"BMP",  FORMAT_KIND_RASTER },
    { L".dib",  L"BMP",  FORMAT_KIND_RASTER },
    { L".gif",  L"GIF",  FORMAT_KIND_RASTER },
    { L".tif",  L"TIFF", FORMAT_KIND_RASTER },
    { L".tiff", L"TIFF", FORMAT_KIND_RASTER },
    { L".ico",  L"ICO",  FORMAT_KIND_RASTER },
    { L".webp", L"WebP", FORMAT_KIND_RASTER },
    { L".heic", L"HEIF", FORMAT_KIND_RASTER },
    { L".heif", L"HEIF", FORMAT_KIND_RASTER },
    { L".avif", L"AVIF", FORMAT_KIND_RASTER },
    { L".svg",  L"SVG",  FORMAT_KIND_SVG },
};

static const ImageFormat *find_format(const wchar_t *fileName)
{
    const wchar_t *ext = fileName ? wcsrchr(fileName, L'.') : NULL;
    if (!ext) return NULL;
    for (size_t i = 0; i < ARRAY_COUNT(FORMATS); ++i) {
        if (_wcsicmp(ext, FORMATS[i].extension) == 0) return &FORMATS[i];
    }
    return NULL;
}

FormatKind format_kind(const wchar_t *fileName)
{
    const ImageFormat *format = find_format(fileName);
    return format ? format->kind : FORMAT_KIND_NONE;
}

bool format_is_supported(const wchar_t *fileName)
{
    return find_format(fileName) != NULL;
}

const wchar_t *format_name(const wchar_t *fileName)
{
    const ImageFormat *format = find_format(fileName);
    if (format) return format->name;

    const wchar_t *ext = fileName ? wcsrchr(fileName, L'.') : NULL;
    return (ext && ext[1]) ? ext + 1 : NULL;
}

wchar_t *format_build_pattern(void)
{
    size_t length = 1;
    for (size_t i = 0; i < ARRAY_COUNT(FORMATS); ++i) {
        length += 2 + wcslen(FORMATS[i].extension); /* "*" + 拡張子 + ";" */
    }

    wchar_t *pattern = malloc(length * sizeof(wchar_t));
    if (!pattern) return NULL;

    pattern[0] = L'\0';
    for (size_t i = 0; i < ARRAY_COUNT(FORMATS); ++i) {
        if (i > 0) wcscat(pattern, L";");
        wcscat(pattern, L"*");
        wcscat(pattern, FORMATS[i].extension);
    }
    return pattern;
}
