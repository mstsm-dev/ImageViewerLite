/*
 * worker.c — デコード用ワーカースレッド
 *
 * スレッドは1本。COM（MTA）の初期化と、WIC / Direct2D ファクトリの生成・解放を担当する。
 * 停止は終了フラグとイベント通知・待機で行い、TerminateThread は使わない。
 */
#include "worker.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    wchar_t *paths[WORKER_MAX_JOBS];
    bool prefetch[WORKER_MAX_JOBS];
    size_t count;
    unsigned long generation;
    bool autoOrientation;
} JobBatch;

static void free_paths(wchar_t **paths, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        free(paths[i]);
        paths[i] = NULL;
    }
}

/* 処理中の世代が古くなった、または停止要求があれば true。 */
static bool is_superseded(DecodeWorker *w, unsigned long generation)
{
    EnterCriticalSection(&w->lock);
    bool superseded = w->stopping || generation != w->generation;
    LeaveCriticalSection(&w->lock);
    return superseded;
}

/* 保留中の要求をバッチへ移す（複製せず所有権を移動）。停止要求なら false。 */
static bool take_pending(DecodeWorker *w, JobBatch *batch)
{
    memset(batch, 0, sizeof(*batch));
    EnterCriticalSection(&w->lock);
    bool running = !w->stopping;
    if (running) {
        for (size_t i = 0; i < w->pendingCount; ++i) {
            batch->paths[i] = w->pendingPaths[i];
            batch->prefetch[i] = w->pendingPrefetch[i];
            w->pendingPaths[i] = NULL;
        }
        batch->count = w->pendingCount;
        batch->generation = w->generation;
        batch->autoOrientation = w->autoOrientation;
        w->pendingCount = 0;
    }
    LeaveCriticalSection(&w->lock);
    return running;
}

static void run_job(DecodeWorker *w, DecoderContext *decoder, HRESULT initResult,
                    const JobBatch *batch, size_t index)
{
    DecodeResult *result = calloc(1, sizeof(*result));
    if (!result) return;

    result->path = str_dup(batch->paths[index]);
    result->generation = batch->generation;
    result->prefetch = batch->prefetch[index];
    if (!result->path) {
        free(result);
        return;
    }

    result->hr = SUCCEEDED(initResult)
        ? decoder_decode(decoder, result->path, batch->autoOrientation, &result->image)
        : initResult;

    /* 待っている間に要求が変わっていれば結果は捨てる */
    if (is_superseded(w, batch->generation)
        || !PostMessageW(w->notifyWindow, w->notifyMessage, 0, (LPARAM)result)) {
        decode_result_free(result);
    }
}

static DWORD WINAPI worker_main(void *param)
{
    DecodeWorker *w = param;

    HRESULT comResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    DecoderContext decoder = { 0 };
    HRESULT initResult = comResult;
    if (SUCCEEDED(comResult)) initResult = decoder_context_init(&decoder);

    for (;;) {
        WaitForSingleObject(w->wakeEvent, INFINITE);

        JobBatch batch;
        if (!take_pending(w, &batch)) break;

        /* 優先順位：現在画像 → 次 → 前（submit 時の並び順） */
        for (size_t i = 0; i < batch.count; ++i) {
            if (is_superseded(w, batch.generation)) break;
            run_job(w, &decoder, initResult, &batch, i);
        }
        free_paths(batch.paths, batch.count);
    }

    decoder_context_free(&decoder);
    if (SUCCEEDED(comResult)) CoUninitialize();
    return 0;
}

bool worker_start(DecodeWorker *w, HWND notifyWindow, UINT notifyMessage)
{
    memset(w, 0, sizeof(*w));
    w->notifyWindow = notifyWindow;
    w->notifyMessage = notifyMessage;

    InitializeCriticalSection(&w->lock);
    w->lockInitialized = true;

    w->wakeEvent = CreateEventW(NULL, FALSE, FALSE, NULL); /* 自動リセット */
    if (!w->wakeEvent) {
        worker_stop(w);
        return false;
    }
    w->thread = CreateThread(NULL, 0, worker_main, w, 0, NULL);
    if (!w->thread) {
        worker_stop(w);
        return false;
    }
    return true;
}

void worker_stop(DecodeWorker *w)
{
    if (!w->lockInitialized) return;

    if (w->thread) {
        EnterCriticalSection(&w->lock);
        w->stopping = true;
        LeaveCriticalSection(&w->lock);
        SetEvent(w->wakeEvent);
        WaitForSingleObject(w->thread, INFINITE);
        CloseHandle(w->thread);
        w->thread = NULL;
    }
    if (w->wakeEvent) {
        CloseHandle(w->wakeEvent);
        w->wakeEvent = NULL;
    }
    free_paths(w->pendingPaths, w->pendingCount);
    w->pendingCount = 0;

    DeleteCriticalSection(&w->lock);
    w->lockInitialized = false;

    /* 投函済みで UI 側がまだ受け取っていない結果を解放する */
    if (w->notifyWindow) {
        MSG msg;
        while (PeekMessageW(&msg, w->notifyWindow, w->notifyMessage, w->notifyMessage, PM_REMOVE)) {
            decode_result_free((DecodeResult *)msg.lParam);
        }
    }
}

void worker_submit(DecodeWorker *w, unsigned long generation,
                   const DecodeJob *jobs, size_t count, bool autoOrientation)
{
    if (!w->thread) return;
    if (count > WORKER_MAX_JOBS) count = WORKER_MAX_JOBS;

    EnterCriticalSection(&w->lock);
    free_paths(w->pendingPaths, w->pendingCount);
    w->pendingCount = 0;
    for (size_t i = 0; i < count; ++i) {
        wchar_t *copy = str_dup(jobs[i].path);
        if (!copy) continue;
        w->pendingPaths[w->pendingCount] = copy;
        w->pendingPrefetch[w->pendingCount] = jobs[i].prefetch;
        w->pendingCount++;
    }
    w->generation = generation;
    w->autoOrientation = autoOrientation;
    LeaveCriticalSection(&w->lock);

    SetEvent(w->wakeEvent);
}

void decode_result_free(DecodeResult *result)
{
    if (!result) return;
    decoded_image_free(&result->image);
    free(result->path);
    free(result);
}
