/*
 * cache.h — デコード済み画像の LRU キャッシュ（仕様書 7章）
 *
 * ファイルパスをキーとする。UI スレッド専用。
 * 「固定パス」（現在表示中の画像）は容量上限を超えても追い出さない。
 */
#ifndef IVL_CACHE_H
#define IVL_CACHE_H

#include "common.h"
#include "decoder.h"

/* 現在画像＋前後の先読み＋直前に見ていた画像を保持できる数 */
#define CACHE_SLOTS 5

typedef struct {
    wchar_t *path;
    DecodedImage image;
    unsigned long long lastUsed;
} CacheEntry;

typedef struct {
    CacheEntry slots[CACHE_SLOTS];
    unsigned long long clock;
    size_t maxBytes;
} ImageCache;

/* 見つかれば画像を返し、最終使用時刻を更新する。 */
const DecodedImage *cache_get(ImageCache *cache, const wchar_t *path);

/* 最終使用時刻を更新せずに存在だけ確認する。 */
bool cache_contains(const ImageCache *cache, const wchar_t *path);

/*
 * 画像を登録する。image の所有権は常にキャッシュへ移る（失敗時も解放される）。
 * pinnedPath は追い出し対象から外す。既に同じパスがあれば何もせず true。
 */
bool cache_put(ImageCache *cache, const wchar_t *path, DecodedImage *image, const wchar_t *pinnedPath);

void cache_clear(ImageCache *cache);

#endif /* IVL_CACHE_H */
