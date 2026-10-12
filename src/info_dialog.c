/*
 * info_dialog.c — 画像情報ダイアログ
 *
 * 既存のファイル一覧とキャッシュの値だけを表示し、画像の再デコードはしない。
 * 表示しない項目：解像度（DPI）、印刷サイズ、使用色数、読み込み時間。
 */
#include "dialogs.h"
#include "formats.h"

#include <stdio.h>
#include <wchar.h>

#define TEXT_CAPACITY 256

static void set_field(HWND dialog, int id, const wchar_t *value)
{
    SetDlgItemTextW(dialog, id, value ? value : L"");
}

static void set_res_field(HWND dialog, int id, UINT stringId)
{
    wchar_t text[TEXT_CAPACITY];
    set_field(dialog, id, res_string(stringId, text, TEXT_CAPACITY));
}

/* "1.23 MiB (1234567 バイト)" 形式。書式は STRINGTABLE に置く。 */
static void format_bytes(wchar_t *out, size_t capacity, ULONGLONG bytes)
{
    wchar_t format[TEXT_CAPACITY];
    double kib = (double)bytes / 1024.0;
    if (kib >= 1024.0) {
        res_string(IDS_INFO_BYTES_MIB, format, TEXT_CAPACITY);
        swprintf(out, capacity, format, kib / 1024.0, (unsigned long long)bytes);
    } else {
        res_string(IDS_INFO_BYTES_KIB, format, TEXT_CAPACITY);
        swprintf(out, capacity, format, kib, (unsigned long long)bytes);
    }
}

static void format_local_time(wchar_t *out, size_t capacity, const FILETIME *utc, bool *ok)
{
    FILETIME local;
    SYSTEMTIME st;
    *ok = FileTimeToLocalFileTime(utc, &local) && FileTimeToSystemTime(&local, &st);
    if (*ok) {
        swprintf(out, capacity, L"%04u/%02u/%02u %02u:%02u:%02u",
                 (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
                 (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);
    }
}

static void populate_file_fields(HWND dialog, const ImageInfo *info)
{
    wchar_t text[TEXT_CAPACITY];
    const FileEntry *file = info->file;

    set_field(dialog, IDC_INFO_FILENAME, file->name);
    set_field(dialog, IDC_INFO_DIRECTORY, info->directory);
    set_field(dialog, IDC_INFO_FULLPATH, file->path);

    const wchar_t *format = format_name(file->name);
    if (format) set_field(dialog, IDC_INFO_FORMAT, format);
    else set_res_field(dialog, IDC_INFO_FORMAT, IDS_INFO_UNKNOWN);

    format_bytes(text, TEXT_CAPACITY, file->size);
    set_field(dialog, IDC_INFO_DISKSIZE, text);

    swprintf(text, TEXT_CAPACITY, L"%zu / %zu", info->position, info->count);
    set_field(dialog, IDC_INFO_POSITION, text);

    bool ok;
    format_local_time(text, TEXT_CAPACITY, &file->modified, &ok);
    if (ok) set_field(dialog, IDC_INFO_MODIFIED, text);
    else set_res_field(dialog, IDC_INFO_MODIFIED, IDS_INFO_UNKNOWN);
}

static void populate_image_fields(HWND dialog, const ImageInfo *info)
{
    const DecodedImage *image = info->image;
    if (!image) {
        set_res_field(dialog, IDC_INFO_ORIGINAL, info->loading ? IDS_INFO_LOADING : IDS_INFO_UNAVAILABLE);
        set_res_field(dialog, IDC_INFO_CURRENT, IDS_INFO_NONE);
        set_res_field(dialog, IDC_INFO_MEMORYSIZE, IDS_INFO_NONE);
        return;
    }

    wchar_t format[TEXT_CAPACITY];
    wchar_t text[TEXT_CAPACITY];

    res_string(IDS_INFO_PIXELS, format, TEXT_CAPACITY);
    swprintf(text, TEXT_CAPACITY, format, image->width, image->height);
    set_field(dialog, IDC_INFO_ORIGINAL, text);

    /* 「現在のサイズ」は倍率適用後の見かけの寸法（ファイルはリサイズしない） */
    res_string(IDS_INFO_ZOOMED, format, TEXT_CAPACITY);
    swprintf(text, TEXT_CAPACITY, format,
             image->width * info->zoom, image->height * info->zoom, info->zoom * 100.0);
    set_field(dialog, IDC_INFO_CURRENT, text);

    format_bytes(text, TEXT_CAPACITY, (ULONGLONG)image->bytes);
    set_field(dialog, IDC_INFO_MEMORYSIZE, text);
}

static INT_PTR CALLBACK info_dialog_proc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG: {
        const ImageInfo *info = (const ImageInfo *)lParam;
        populate_file_fields(dialog, info);
        populate_image_fields(dialog, info);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void info_dialog_show(HWND owner, const ImageInfo *info)
{
    if (!info || !info->file) return;
    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_IMAGEINFO), owner,
                    info_dialog_proc, (LPARAM)info);
}
