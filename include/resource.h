/*
 * resource.h — リソースIDの一覧
 *
 * C ソースとリソーススクリプト（windres）の両方から読み込まれる。
 * 番号帯：
 *   100番台 アイコン / 200番台 メニュー・アクセラレータ / 300番台 ダイアログ
 *   400番台 設定ダイアログの部品 / 600番台 画像情報ダイアログの部品
 *   1000番台 コマンド（メニュー・アクセラレータ共通）
 *   2000番台以降 文字列（STRINGTABLE）
 */
#ifndef IVL_RESOURCE_H
#define IVL_RESOURCE_H

/* ---- アイコン ---- */
#define IDI_APP                 101

/* ---- メニュー・アクセラレータ ---- */
#define IDR_MAINMENU            201
#define IDR_POPUPMENU           202
#define IDR_ACCELERATORS        203

/* ---- ダイアログ ---- */
#define IDD_SETTINGS            301
#define IDD_IMAGEINFO           302

/* ---- 設定ダイアログの部品 ---- */
#define IDC_FIT                 401
#define IDC_BACKGROUND          402
#define IDC_QUALITY             403
#define IDC_ZOOMLOCK            404
#define IDC_LOOP                405

/* ---- 画像情報ダイアログの部品（表示順） ---- */
#define IDC_INFO_FILENAME       601
#define IDC_INFO_DIRECTORY      602
#define IDC_INFO_FULLPATH       603
#define IDC_INFO_FORMAT         604
#define IDC_INFO_ORIGINAL       605
#define IDC_INFO_CURRENT        606
#define IDC_INFO_DISKSIZE       607
#define IDC_INFO_MEMORYSIZE     608
#define IDC_INFO_POSITION       609
#define IDC_INFO_MODIFIED       610

/* ---- コマンド：ファイル ---- */
#define IDM_OPEN                1001
#define IDM_RELOAD              1002
#define IDM_EXIT                1003
#define IDM_IMAGEINFO           1004
#define IDM_COPY                1005
#define IDM_ESCAPE              1006 /* Esc キー専用：全画面なら解除、それ以外は終了 */

/* ---- コマンド：移動 ---- */
#define IDM_PREV                1010
#define IDM_NEXT                1011
#define IDM_FIRST               1012
#define IDM_LAST                1013

/* ---- コマンド：表示 ---- */
#define IDM_ZOOMIN              1020
#define IDM_ZOOMOUT             1021
#define IDM_ACTUAL              1022
#define IDM_SHRINK              1023
#define IDM_FIT                 1024
#define IDM_WIDTH               1025
#define IDM_HEIGHT              1026
#define IDM_LOCK                1027
#define IDM_FULLSCREEN          1028
#define IDM_SIMPLE              1029

/* ---- コマンド：設定・ヘルプ ---- */
#define IDM_SETTINGS            1030
#define IDM_ABOUT               1031

/* ---- コマンド：並べ替え（SortMode の並びと一致させる） ---- */
#define IDM_SORT_NATURAL        1040
#define IDM_SORT_NAME           1041
#define IDM_SORT_DATE           1042
#define IDM_SORT_SIZE           1043
#define IDM_SORT_DESC           1044

/* ---- 文字列：メッセージ ---- */
#define IDS_ABOUT_FORMAT        2001
#define IDS_CLIPBOARD_ERROR     2002
#define IDS_SAVE_ERROR          2003
#define IDS_WORKER_ERROR        2004
#define IDS_INIT_ERROR          2005
#define IDS_FOLDER_EMPTY        2006
#define IDS_OPEN_FAILED         2007

/* ---- 文字列：画面表示 ---- */
#define IDS_PLACEHOLDER_EMPTY   2101
#define IDS_PLACEHOLDER_LOADING 2102
#define IDS_PLACEHOLDER_ERROR   2103
#define IDS_STATUS_LOADING      2104
#define IDS_STATUS_FAILED       2105

/* ---- 文字列：ファイル選択ダイアログ ---- */
#define IDS_FILTER_IMAGES       2201
#define IDS_FILTER_ALL          2202

/* ---- 文字列：設定ダイアログの選択肢（順番に意味がある） ---- */
#define IDS_FIT_FIRST           2301 /* FitMode の FIT_SHRINK〜FIT_HEIGHT と同順 */
#define IDS_FIT_SHRINK          2301
#define IDS_FIT_ALL             2302
#define IDS_FIT_ACTUAL          2303
#define IDS_FIT_WIDTH           2304
#define IDS_FIT_HEIGHT          2305
#define IDS_BG_DARK             2311
#define IDS_BG_BLACK            2312
#define IDS_BG_CHECKER          2313
#define IDS_QUALITY_HIGH        2321
#define IDS_QUALITY_FAST        2322

/* ---- 文字列：画像情報ダイアログ ---- */
#define IDS_INFO_LOADING        2401
#define IDS_INFO_UNAVAILABLE    2402
#define IDS_INFO_NONE           2403
#define IDS_INFO_UNKNOWN        2404
#define IDS_INFO_PIXELS         2405 /* 書式: %d × %d */
#define IDS_INFO_ZOOMED         2406 /* 書式: %.0f × %.0f (%.1f%%) */
#define IDS_INFO_BYTES_KIB      2407 /* 書式: %.2f KiB (%llu) */
#define IDS_INFO_BYTES_MIB      2408 /* 書式: %.2f MiB (%llu) */

#endif /* IVL_RESOURCE_H */
