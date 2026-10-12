/*
 * version.h — 版数の唯一の定義場所
 *
 * C ソースとリソーススクリプト（windres）の両方から読み込まれるため、
 * #define 以外の記述（型宣言など）を置かないこと。
 * 版数を上げるときは数値と文字列の両方を更新する。
 */
#ifndef IVL_VERSION_H
#define IVL_VERSION_H

#define IVL_VERSION_MAJOR 1
#define IVL_VERSION_MINOR 0
#define IVL_VERSION_PATCH 0
#define IVL_VERSION_STRING "1.0.0"

#define IVL_APP_NAME_STRING "ImageViewerLite"
#define IVL_EXE_NAME_STRING "ImageViewerLite.exe"

#endif /* IVL_VERSION_H */
