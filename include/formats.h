/*
 * formats.h — 対応拡張子と形式名の一覧（仕様書 4.1）
 *
 * 一覧の読み込み対象、ファイル選択ダイアログのフィルタ、ステータスバーと
 * 画像情報ダイアログの形式名は、すべて formats.c の表から作る。
 * 拡張子を追加するときは表に1行足すだけでよい。
 */
#ifndef IVL_FORMATS_H
#define IVL_FORMATS_H

#include "common.h"

typedef enum {
    FORMAT_KIND_NONE,   /* 非対応 */
    FORMAT_KIND_RASTER, /* WIC でデコード */
    FORMAT_KIND_SVG     /* Direct2D で描画（svg.c） */
} FormatKind;

/* 拡張子からデコード方式を返す。 */
FormatKind format_kind(const wchar_t *fileName);

/* ファイル名の拡張子が対応形式なら true。 */
bool format_is_supported(const wchar_t *fileName);

/* 拡張子から形式名（"JPEG" など）を返す。未知の拡張子は拡張子そのもの、
 * 拡張子がなければ NULL。※実際の符号化形式ではなく拡張子による判定。 */
const wchar_t *format_name(const wchar_t *fileName);

/* GetOpenFileNameW 用のフィルタ文字列（"*.jpg;*.jpeg;..."）を作る。
 * 呼び出し側で free。 */
wchar_t *format_build_pattern(void);

#endif /* IVL_FORMATS_H */
