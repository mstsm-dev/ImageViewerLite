/*
 * about_dialog.c — バージョン情報ダイアログ
 *
 * MessageBox に情報アイコンを付けると Windows のシステム音が鳴るため、
 * 音の鳴らない通常のダイアログ（resource.rc の IDD_ABOUT）で表示する。
 * 固定文言とレイアウトは RC、アプリ名と版数だけを C から設定する。
 */
#include "dialogs.h"

#include <stdio.h>

static INT_PTR CALLBACK about_dialog_proc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG: {
        wchar_t format[64];
        wchar_t title[128];
        res_string(IDS_ABOUT_FORMAT, format, ARRAY_COUNT(format));
        swprintf(title, ARRAY_COUNT(title), format, APP_NAME, APP_VERSION);
        SetDlgItemTextW(dialog, IDC_ABOUT_TITLE, title);
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

void about_dialog_show(HWND owner)
{
    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT), owner, about_dialog_proc, 0);
}
