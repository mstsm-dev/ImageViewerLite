/*
 * settings_dialog.c — 表示設定ダイアログ（初期倍率・背景・描画品質・ズーム固定・ループ）
 */
#include "dialogs.h"

#include <windowsx.h>

/* 背景の選択肢。checkerboard=true のとき背景色自体は変更しない。 */
typedef struct {
    UINT labelId;
    COLORREF color;
    bool checkerboard;
} BackgroundPreset;

static const BackgroundPreset BACKGROUNDS[] = {
    { IDS_BG_DARK,    RGB(0x20, 0x20, 0x20), false },
    { IDS_BG_BLACK,   RGB(0x00, 0x00, 0x00), false },
    { IDS_BG_CHECKER, 0,                     true  },
};

static const UINT QUALITY_LABELS[] = { IDS_QUALITY_HIGH, IDS_QUALITY_FAST }; /* 0: 高品質 */

typedef struct {
    Settings *settings;
    int initialBackground; /* 開いた時点の選択。変更がなければ INI の任意色を保つ */
} DialogState;

static void add_choice(HWND combo, UINT labelId)
{
    wchar_t label[128];
    ComboBox_AddString(combo, res_string(labelId, label, ARRAY_COUNT(label)));
}

static int background_index(const Settings *s)
{
    for (int i = 0; i < (int)ARRAY_COUNT(BACKGROUNDS); ++i) {
        if (BACKGROUNDS[i].checkerboard ? s->checkerboard
                                        : (!s->checkerboard && s->background == BACKGROUNDS[i].color)) {
            return i;
        }
    }
    return 0; /* INI で任意色が指定されている場合は先頭を表示 */
}

static void load_controls(HWND dialog, DialogState *state)
{
    const Settings *s = state->settings;

    HWND fit = GetDlgItem(dialog, IDC_FIT);
    for (int i = 0; i < FIT_INITIAL_COUNT; ++i) add_choice(fit, IDS_FIT_FIRST + (UINT)i);
    ComboBox_SetCurSel(fit, s->initialFit < FIT_INITIAL_COUNT ? (int)s->initialFit : 0);

    HWND background = GetDlgItem(dialog, IDC_BACKGROUND);
    for (size_t i = 0; i < ARRAY_COUNT(BACKGROUNDS); ++i) add_choice(background, BACKGROUNDS[i].labelId);
    state->initialBackground = background_index(s);
    ComboBox_SetCurSel(background, state->initialBackground);

    HWND quality = GetDlgItem(dialog, IDC_QUALITY);
    for (size_t i = 0; i < ARRAY_COUNT(QUALITY_LABELS); ++i) add_choice(quality, QUALITY_LABELS[i]);
    ComboBox_SetCurSel(quality, s->highQuality ? 0 : 1);

    CheckDlgButton(dialog, IDC_ZOOMLOCK, s->zoomLock ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_LOOP, s->loop ? BST_CHECKED : BST_UNCHECKED);
}

/* 入力値を検証して設定へ反映する。不正なら false（ダイアログを閉じない）。 */
static bool store_controls(HWND dialog, DialogState *state)
{
    int fit = ComboBox_GetCurSel(GetDlgItem(dialog, IDC_FIT));
    int background = ComboBox_GetCurSel(GetDlgItem(dialog, IDC_BACKGROUND));
    int quality = ComboBox_GetCurSel(GetDlgItem(dialog, IDC_QUALITY));
    if (fit < 0 || fit >= FIT_INITIAL_COUNT) return false;
    if (background < 0 || background >= (int)ARRAY_COUNT(BACKGROUNDS)) return false;
    if (quality < 0 || quality >= (int)ARRAY_COUNT(QUALITY_LABELS)) return false;

    Settings *s = state->settings;
    s->initialFit = (FitMode)fit;
    if (background != state->initialBackground) {
        const BackgroundPreset *preset = &BACKGROUNDS[background];
        s->checkerboard = preset->checkerboard;
        if (!preset->checkerboard) s->background = preset->color;
    }
    s->highQuality = (quality == 0);
    s->zoomLock = IsDlgButtonChecked(dialog, IDC_ZOOMLOCK) == BST_CHECKED;
    s->loop = IsDlgButtonChecked(dialog, IDC_LOOP) == BST_CHECKED;
    return true;
}

static INT_PTR CALLBACK settings_dialog_proc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    DialogState *state = (DialogState *)GetWindowLongPtrW(dialog, DWLP_USER);

    switch (message) {
    case WM_INITDIALOG:
        state = (DialogState *)lParam;
        SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)state);
        load_controls(dialog, state);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (store_controls(dialog, state)) EndDialog(dialog, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

bool settings_dialog_show(HWND owner, Settings *settings)
{
    /* キャンセル時に元の設定を変更しないよう、複製を編集する */
    Settings edited = *settings;
    DialogState state = { &edited, 0 };
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SETTINGS), owner,
                                     settings_dialog_proc, (LPARAM)&state);
    if (result != IDOK) return false;
    *settings = edited;
    return true;
}
