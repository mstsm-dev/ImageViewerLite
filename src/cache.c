/*
 * cache.c — デコード済み画像の LRU キャッシュ
 */
#include "cache.h"

#include <stdlib.h>
#include <string.h>

#define NOT_FOUND SIZE_MAX

static size_t find_index(const ImageCache *cache, const wchar_t *path)
{
    if (!path) return NOT_FOUND;
    for (size_t i = 0; i < CACHE_SLOTS; ++i) {
        if (path_equals(cache->slots[i].path, path)) return i;
    }
    return NOT_FOUND;
}

static CacheEntry *find_entry(ImageCache *cache, const wchar_t *path)
{
    size_t index = find_index(cache, path);
    return index == NOT_FOUND ? NULL : &cache->slots[index];
}

static void release_entry(CacheEntry *entry)
{
    decoded_image_free(&entry->image);
    free(entry->path);
    memset(entry, 0, sizeof(*entry));
}

static size_t used_bytes(const ImageCache *cache)
{
    size_t total = 0;
    for (size_t i = 0; i < CACHE_SLOTS; ++i) total += cache->slots[i].image.bytes;
    return total;
}

/* 固定パス以外で最も古い使用中スロット。なければ NULL。 */
static CacheEntry *least_recently_used(ImageCache *cache, const wchar_t *pinnedPath)
{
    CacheEntry *victim = NULL;
    for (size_t i = 0; i < CACHE_SLOTS; ++i) {
        CacheEntry *entry = &cache->slots[i];
        if (!entry->path || path_equals(entry->path, pinnedPath)) continue;
        if (!victim || entry->lastUsed < victim->lastUsed) victim = entry;
    }
    return victim;
}

/* 容量上限を超えている間、古いものから追い出す。 */
static void trim_to_limit(ImageCache *cache, const wchar_t *pinnedPath)
{
    size_t used = used_bytes(cache);
    while (used > cache->maxBytes) {
        CacheEntry *victim = least_recently_used(cache, pinnedPath);
        if (!victim) break;
        used -= victim->image.bytes;
        release_entry(victim);
    }
}

const DecodedImage *cache_get(ImageCache *cache, const wchar_t *path)
{
    CacheEntry *entry = find_entry(cache, path);
    if (!entry) return NULL;
    entry->lastUsed = ++cache->clock;
    return &entry->image;
}

bool cache_contains(const ImageCache *cache, const wchar_t *path)
{
    return find_index(cache, path) != NOT_FOUND;
}

bool cache_put(ImageCache *cache, const wchar_t *path, DecodedImage *image, const wchar_t *pinnedPath)
{
    if (!image->bitmap) return false;
    if (find_entry(cache, path)) {
        decoded_image_free(image);
        return true;
    }

    /* 空きスロット、なければ固定パス以外の最古スロットを使う */
    CacheEntry *slot = NULL;
    for (size_t i = 0; !slot && i < CACHE_SLOTS; ++i) {
        if (!cache->slots[i].path) slot = &cache->slots[i];
    }
    if (!slot) slot = least_recently_used(cache, pinnedPath);

    wchar_t *key = slot ? str_dup(path) : NULL;
    if (!key) {
        decoded_image_free(image);
        return false;
    }

    release_entry(slot);
    slot->path = key;
    slot->image = *image;
    slot->lastUsed = ++cache->clock;
    memset(image, 0, sizeof(*image)); /* 所有権を移した */

    trim_to_limit(cache, pinnedPath);
    return find_entry(cache, path) != NULL;
}

void cache_clear(ImageCache *cache)
{
    for (size_t i = 0; i < CACHE_SLOTS; ++i) release_entry(&cache->slots[i]);
}
