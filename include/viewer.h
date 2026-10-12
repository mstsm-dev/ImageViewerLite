#ifndef LITEVIEWER_H
#define LITEVIEWER_H
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define APP_NAME L"LiteViewer"
#define WM_DECODED (WM_APP + 1)
#define MAX_IMAGE_BYTES (1024ULL*1024ULL*1024ULL)
#define CACHE_SLOTS 5

typedef enum { FIT_SHRINK, FIT_ALL, FIT_ACTUAL, FIT_WIDTH, FIT_HEIGHT, FIT_MANUAL } FitMode;
typedef enum { SORT_NATURAL, SORT_NAME, SORT_DATE, SORT_SIZE } SortMode;
typedef enum { VIEW_NORMAL, VIEW_SIMPLE, VIEW_FULLSCREEN } WindowMode;
typedef struct { wchar_t *path; wchar_t *name; ULONGLONG size; FILETIME modified; } FileEntry;
typedef struct { FileEntry *items; size_t count, capacity, index; wchar_t *directory; } FileList;
typedef struct { int width, height, stride; BYTE *pixels; BOOL hasAlpha; } Decoded;
typedef struct { wchar_t *path; HBITMAP bitmap; HDC dc; void *bits; int w,h; BOOL alpha; size_t bytes; unsigned long long tick; } CacheEntry;
typedef struct {
  FitMode initialFit; SortMode sort; BOOL descending, loop, zoomLock;
  BOOL highQuality, checkerboard, autoOrientation, statusbar;
  COLORREF background; double zoomStep; size_t maxCacheBytes;
  int x,y,width,height; BOOL maximized; WindowMode startMode;
} Settings;
typedef struct { wchar_t *path; unsigned long generation; BOOL prefetch; HRESULT hr; Decoded image; } DecodeResult;
typedef struct {
  HWND hwnd, status, settingsWindow; HWND settingControls[5];
  HDC backDC; HBITMAP backBitmap; HBITMAP backOld; int backW,backH;
  Settings cfg; wchar_t iniPath[32768]; FileList files;
  CacheEntry cache[CACHE_SLOTS]; unsigned long long cacheClock;
  wchar_t *selectedPath; unsigned long generation; BOOL loading, error;
  double zoom, panX, panY; FitMode fit; BOOL zoomLock;
  WindowMode mode; WINDOWPLACEMENT oldPlacement; LONG_PTR oldStyle,oldExStyle;
  BOOL dragging; POINT lastMouse; int wheelDelta;
  HANDLE worker, event; CRITICAL_SECTION requestLock; BOOL stopping;
  wchar_t *requested; unsigned long requestGeneration; BOOL requestSkipCurrent;
  wchar_t *requestNext, *requestPrev;
} App;

wchar_t *dupw(const wchar_t *s);
void settings_defaults(Settings *s);
void settings_load(App *a);
BOOL settings_save(App *a);
BOOL files_open(FileList *f, const wchar_t *filepath, SortMode mode, BOOL descending);
void files_close(FileList *f);
BOOL files_directory_first(wchar_t **first, const wchar_t *directory);
HRESULT decode_wic(const wchar_t *path, BOOL orientation, Decoded *out);
void decoded_free(Decoded *d);
DWORD WINAPI decode_worker(void *p);
void worker_request(App *a, BOOL skipCurrent);
void worker_stop(App *a);
void app_open(App *a, const wchar_t *path);
void app_navigate(App *a, int change, BOOL absolute);
void app_reload(App *a);
void app_received(App *a, DecodeResult *result);
void app_render(App *a, HDC dc, int w, int h);
void app_zoom(App *a, double factor, int x, int y);
void app_fit(App *a, FitMode mode);
void app_refresh_ui(App *a);
void app_show_settings(App *a);
void app_toggle_mode(App *a, WindowMode mode);
void app_clear_cache(App *a);
void app_resize_backbuffer(App *a, int w, int h);
void app_paint(App *a);
void app_update_status(App *a);

#define IDM_OPEN 1001
#define IDM_RELOAD 1002
#define IDM_EXIT 1003
#define IDM_PREV 1010
#define IDM_NEXT 1011
#define IDM_FIRST 1012
#define IDM_LAST 1013
#define IDM_ZOOMIN 1020
#define IDM_ZOOMOUT 1021
#define IDM_ACTUAL 1022
#define IDM_SHRINK 1023
#define IDM_FIT 1024
#define IDM_WIDTH 1025
#define IDM_HEIGHT 1026
#define IDM_LOCK 1027
#define IDM_FULLSCREEN 1028
#define IDM_SIMPLE 1029
#define IDM_SETTINGS 1030
#define IDM_ABOUT 1031
#define IDM_SORT_NATURAL 1040
#define IDM_SORT_NAME 1041
#define IDM_SORT_DATE 1042
#define IDM_SORT_SIZE 1043
#define IDM_SORT_DESC 1044
#endif
