/*
 * decoder.c — 画像デコード（WIC によるラスター画像と、SVG への振り分け）
 */
#include "decoder.h"
#include "formats.h"
#include "svg.h"

#include <limits.h>
#include <propidl.h>
#include <string.h>

/* JPEG の EXIF Orientation（タグ 274） */
static const wchar_t ORIENTATION_QUERY[] = L"/app1/ifd/{ushort=274}";

void decoded_image_free(DecodedImage *image)
{
    if (!image) return;
    if (image->bitmap) DeleteObject(image->bitmap);
    memset(image, 0, sizeof(*image));
}

/* EXIF Orientation を読む。タグがない・読めないときは 1（補正なし）。 */
static UINT read_orientation(IWICBitmapFrameDecode *frame)
{
    UINT orientation = 1;
    IWICMetadataQueryReader *reader = NULL;
    if (FAILED(IWICBitmapFrameDecode_GetMetadataQueryReader(frame, &reader))) return orientation;

    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(IWICMetadataQueryReader_GetMetadataByName(reader, ORIENTATION_QUERY, &value))) {
        if (value.vt == VT_UI2) orientation = value.uiVal;
        else if (value.vt == VT_UI4) orientation = value.ulVal;
    }
    PropVariantClear(&value);
    IWICMetadataQueryReader_Release(reader);
    return orientation;
}

/* EXIF Orientation（1〜8）を WIC の変換指定に対応づける。 */
static WICBitmapTransformOptions orientation_transform(UINT orientation)
{
    switch (orientation) {
    case 2: return WICBitmapTransformFlipHorizontal;
    case 3: return WICBitmapTransformRotate180;
    case 4: return WICBitmapTransformFlipVertical;
    case 5: return WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal;
    case 6: return WICBitmapTransformRotate90;
    case 7: return WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal;
    case 8: return WICBitmapTransformRotate270;
    default: return WICBitmapTransformRotate0;
    }
}

/* ------------------------------------------------------------------------
 * DecodedImage
 * ------------------------------------------------------------------------ */

HRESULT decoded_image_create(int width, int height, DecodedImage *out)
{
    memset(out, 0, sizeof(*out));
    if (width <= 0 || height <= 0 || width > INT_MAX / 4) return E_INVALIDARG;

    uint64_t bytes = (uint64_t)width * 4 * (uint64_t)height;
    if (bytes > DECODER_MAX_IMAGE_BYTES || bytes > UINT_MAX) return E_OUTOFMEMORY;

    BITMAPINFO info = { 0 };
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; /* 負の値でトップダウン */
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        return E_OUTOFMEMORY;
    }
    out->bitmap = bitmap;
    out->bits = bits;
    out->width = width;
    out->height = height;
    out->bytes = (size_t)bytes;
    return S_OK;
}

void decoded_image_finish(DecodedImage *image)
{
    GdiFlush();
    const BYTE *pixels = image->bits;
    image->hasAlpha = false;
    for (size_t i = 3; i < image->bytes; i += 4) {
        if (pixels[i] != 0xFF) {
            image->hasAlpha = true;
            break;
        }
    }
}

/* ------------------------------------------------------------------------
 * WIC（ラスター画像）
 * ------------------------------------------------------------------------ */

static HRESULT decode_wic(IWICImagingFactory *factory, const wchar_t *path, bool autoOrientation,
                          DecodedImage *out)
{
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICBitmapFlipRotator *rotator = NULL;
    IWICFormatConverter *converter = NULL;
    IWICBitmapSource *source = NULL;
    UINT orientation = 1;
    UINT width = 0, height = 0;

    HRESULT hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) goto done;

    /* GIF / TIFF / ICO も先頭フレームのみ */
    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr)) goto done;
    source = (IWICBitmapSource *)frame;

    /* 表示向きの補正 */
    if (autoOrientation) orientation = read_orientation(frame);
    if (orientation >= 2 && orientation <= 8) {
        hr = IWICImagingFactory_CreateBitmapFlipRotator(factory, &rotator);
        if (FAILED(hr)) goto done;
        hr = IWICBitmapFlipRotator_Initialize(rotator, source, orientation_transform(orientation));
        if (FAILED(hr)) goto done;
        source = (IWICBitmapSource *)rotator;
    }

    /* 32bpp premultiplied BGRA へ変換 */
    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr)) goto done;
    hr = IWICFormatConverter_Initialize(converter, source, &GUID_WICPixelFormat32bppPBGRA,
                                        WICBitmapDitherTypeNone, NULL, 0.0,
                                        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto done;

    /* 寸法とバッファサイズの検査を通った場合だけ DIB セクションへ直接展開 */
    hr = IWICFormatConverter_GetSize(converter, &width, &height);
    if (FAILED(hr)) goto done;
    if (width > INT_MAX || height > INT_MAX) {
        hr = E_INVALIDARG;
        goto done;
    }
    hr = decoded_image_create((int)width, (int)height, out);
    if (FAILED(hr)) goto done;
    hr = IWICFormatConverter_CopyPixels(converter, NULL, width * 4, (UINT)out->bytes, out->bits);
    if (FAILED(hr)) goto done;
    decoded_image_finish(out);

done:
    if (FAILED(hr)) decoded_image_free(out);
    if (converter) IWICFormatConverter_Release(converter);
    if (rotator) IWICBitmapFlipRotator_Release(rotator);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    return hr;
}

/* ------------------------------------------------------------------------
 * 公開関数
 * ------------------------------------------------------------------------ */

HRESULT decoder_context_init(DecoderContext *context)
{
    memset(context, 0, sizeof(*context));
    return CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                            &IID_IWICImagingFactory, (void **)&context->wic);
}

void decoder_context_free(DecoderContext *context)
{
    svg_release_factory(context);
    if (context->wic) IWICImagingFactory_Release(context->wic);
    memset(context, 0, sizeof(*context));
}

HRESULT decoder_decode(DecoderContext *context, const wchar_t *path, bool autoOrientation,
                       DecodedImage *out)
{
    memset(out, 0, sizeof(*out));
    if (!context->wic) return E_POINTER;

    if (format_kind(path) == FORMAT_KIND_SVG) return svg_decode(context, path, out);
    return decode_wic(context->wic, path, autoOrientation, out);
}
