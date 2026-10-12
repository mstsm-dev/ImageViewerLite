/*
 * types.h — 複数モジュールで共有する列挙型
 *
 * 値の並びは次のものと対応しているため、変更時は合わせて修正する：
 *   FitMode  … resource.h の IDS_FIT_*（設定ダイアログの選択肢順）
 *   SortMode … resource.h の IDM_SORT_*（メニューのコマンドID順）
 */
#ifndef IVL_TYPES_H
#define IVL_TYPES_H

typedef enum {
    FIT_SHRINK,  /* 縮小のみフィット（表示領域より大きい画像だけ縮小） */
    FIT_ALL,     /* 全体フィット */
    FIT_ACTUAL,  /* 原寸 100% */
    FIT_WIDTH,   /* 幅フィット */
    FIT_HEIGHT,  /* 高さフィット */
    FIT_MANUAL,  /* 手動ズーム後（初期倍率としては選べない） */
    FIT_INITIAL_COUNT = FIT_MANUAL /* 初期倍率として選べる数 */
} FitMode;

typedef enum {
    SORT_NATURAL, /* 自然順（StrCmpLogicalW） */
    SORT_NAME,    /* 名前順 */
    SORT_DATE,    /* 更新日時順 */
    SORT_SIZE,    /* サイズ順 */
    SORT_COUNT
} SortMode;

typedef enum {
    VIEW_NORMAL,     /* タイトル・メニュー・ステータスバーあり */
    VIEW_SIMPLE,     /* タイトルバーと画像のみ */
    VIEW_FULLSCREEN  /* ボーダーレス全画面 */
} WindowMode;

#endif /* IVL_TYPES_H */
