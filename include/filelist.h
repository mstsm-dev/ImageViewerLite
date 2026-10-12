/*
 * filelist.h — 同一フォルダ内の画像ファイル一覧（仕様書 4章）
 */
#ifndef IVL_FILELIST_H
#define IVL_FILELIST_H

#include "common.h"
#include "types.h"

typedef struct {
    wchar_t *path;      /* フルパス */
    wchar_t *name;      /* ファイル名（拡張子付き） */
    ULONGLONG size;     /* バイト数 */
    FILETIME modified;  /* 最終更新日時（UTC） */
} FileEntry;

typedef struct {
    FileEntry *items;
    size_t count;
    size_t capacity;
    size_t index;       /* 現在位置（count > 0 のとき有効） */
    wchar_t *directory; /* 一覧の対象フォルダ */
} FileList;

typedef enum {
    FILELIST_OK,
    FILELIST_NO_IMAGES, /* フォルダに対応画像がない */
    FILELIST_FAILED     /* パス不正・アクセス不可・非対応ファイルなど */
} FileListResult;

/*
 * path がファイルならそのフォルダを列挙して path を現在位置にする。
 * path がフォルダなら直下を列挙して先頭を現在位置にする。
 * 成功時のみ *list を置き換える（失敗時は元の一覧を維持）。
 */
FileListResult filelist_load(FileList *list, const wchar_t *path, SortMode sort, bool descending);

void filelist_free(FileList *list);

/* 現在位置の項目。一覧が空なら NULL。 */
const FileEntry *filelist_current(const FileList *list);

/* index から delta（+1 / -1）進めた位置。端で loop=false なら SIZE_MAX。 */
size_t filelist_step(const FileList *list, size_t index, int delta, bool loop);

#endif /* IVL_FILELIST_H */
