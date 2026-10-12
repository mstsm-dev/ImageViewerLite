/*
 * dialogs.h — 設定ダイアログ・画像情報ダイアログ
 *
 * レイアウトと固定ラベルは resource.rc の DIALOGEX で定義する。
 * C 側は値の入出力と検証だけを担当し、App 全体には依存しない。
 */
#ifndef IVL_DIALOGS_H
#define IVL_DIALOGS_H

#include "common.h"
#include "decoder.h"
#include "filelist.h"
#include "settings.h"

/* 表示設定ダイアログ（仕様書 8.2）。OK なら *settings を更新して true。 */
bool settings_dialog_show(HWND owner, Settings *settings);

/* 画像情報ダイアログに表示する値 */
typedef struct {
    const FileEntry *file;       /* 必須 */
    const wchar_t *directory;
    size_t position;             /* 1 始まり */
    size_t count;
    const DecodedImage *image;   /* 未ロード・ロード失敗なら NULL */
    bool loading;
    double zoom;
} ImageInfo;

/* 画像情報ダイアログ（仕様書 v1.7 追加仕様）。モーダル。 */
void info_dialog_show(HWND owner, const ImageInfo *info);

#endif /* IVL_DIALOGS_H */
