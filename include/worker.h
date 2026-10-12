/*
 * worker.h — デコード用ワーカースレッド（仕様書 7章）
 *
 * UI スレッドは worker_submit() で「世代番号付きのジョブ列」を渡す。
 * 新しい要求が来ると古い要求は破棄され、処理中のジョブも結果を返さない。
 * 結果は notifyMessage（lParam = DecodeResult*）で UI スレッドへ投函され、
 * 受け取った側が decode_result_free() で解放する。
 */
#ifndef IVL_WORKER_H
#define IVL_WORKER_H

#include "common.h"
#include "decoder.h"

/* 1回の要求で渡せるジョブ数（現在画像・次・前） */
#define WORKER_MAX_JOBS 3

typedef struct {
    const wchar_t *path;
    bool prefetch; /* true: 先読み、false: 表示要求 */
} DecodeJob;

typedef struct {
    wchar_t *path;
    unsigned long generation;
    bool prefetch;
    HRESULT hr;
    DecodedImage image; /* SUCCEEDED(hr) のときのみ有効 */
} DecodeResult;

typedef struct {
    HWND notifyWindow;
    UINT notifyMessage;
    HANDLE thread;
    HANDLE wakeEvent;
    CRITICAL_SECTION lock;
    bool lockInitialized;

    /* 以下は lock で保護 */
    bool stopping;
    unsigned long generation;
    bool autoOrientation;
    wchar_t *pendingPaths[WORKER_MAX_JOBS];
    bool pendingPrefetch[WORKER_MAX_JOBS];
    size_t pendingCount;
} DecodeWorker;

bool worker_start(DecodeWorker *worker, HWND notifyWindow, UINT notifyMessage);

/* スレッドに終了を通知して待機し、投函済みで未処理の結果も解放する。 */
void worker_stop(DecodeWorker *worker);

/* 要求を差し替える。jobs[i].path は内部で複製する。 */
void worker_submit(DecodeWorker *worker, unsigned long generation,
                   const DecodeJob *jobs, size_t count, bool autoOrientation);

void decode_result_free(DecodeResult *result);

#endif /* IVL_WORKER_H */
