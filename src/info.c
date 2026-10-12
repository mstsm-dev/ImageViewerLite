#include "viewer.h"
#include <stdio.h>
#include <wchar.h>

/* This dialog uses existing metadata and cache entries; no image re-decoding. */
static void field(HWND h, int id, const wchar_t *value) {
  SetDlgItemTextW(h,id,value ? value : L"");
}
static void byte_size(wchar_t *buf,size_t count, ULONGLONG bytes) {
  double kib=(double)bytes/1024.0;
  if(kib>=1024.0)swprintf(buf,count,L"%.2f MiB (%llu バイト)",kib/1024.0,(unsigned long long)bytes);
  else swprintf(buf,count,L"%.2f KiB (%llu バイト)",kib,(unsigned long long)bytes);
}
static const wchar_t *format_name(const wchar_t *name) {
  const wchar_t *ext=wcsrchr(name,L'.');
  if(!ext)return L"不明";
  if(!_wcsicmp(ext,L".jpg")||!_wcsicmp(ext,L".jpeg")||!_wcsicmp(ext,L".jpe"))return L"JPEG";
  if(!_wcsicmp(ext,L".png"))return L"PNG";
  if(!_wcsicmp(ext,L".bmp")||!_wcsicmp(ext,L".dib"))return L"BMP";
  if(!_wcsicmp(ext,L".gif"))return L"GIF";
  if(!_wcsicmp(ext,L".tif")||!_wcsicmp(ext,L".tiff"))return L"TIFF";
  if(!_wcsicmp(ext,L".ico"))return L"ICO";
  if(!_wcsicmp(ext,L".webp"))return L"WebP";
  if(!_wcsicmp(ext,L".heic")||!_wcsicmp(ext,L".heif"))return L"HEIF";
  if(!_wcsicmp(ext,L".avif"))return L"AVIF";
  return ext+1;
}
static void populate(HWND h,App *a) {
  if(!a->files.count||a->files.index>=a->files.count)return;
  FileEntry *f=&a->files.items[a->files.index];
  wchar_t b[256];
  field(h,IDC_INFO_FILENAME,f->name);
  field(h,IDC_INFO_DIRECTORY,a->files.directory);
  field(h,IDC_INFO_FULLPATH,f->path);
  field(h,IDC_INFO_FORMAT,format_name(f->name));
  CacheEntry *entry=NULL;
  if(a->selectedPath&&!a->loading&&!a->error){
    for(int i=0;i<CACHE_SLOTS;i++)if(a->cache[i].path&&
      !_wcsicmp(a->cache[i].path,a->selectedPath)) {entry=&a->cache[i];break;}
  }
  if(entry){
    swprintf(b,256,L"%d × %d ピクセル",entry->w,entry->h);
    field(h,IDC_INFO_ORIGINAL,b);
    /* 'Current size' means view dimensions after zoom, not a resized file. */
    swprintf(b,256,L"%.0f × %.0f ピクセル (%.1f%%)",entry->w*a->zoom,entry->h*a->zoom,a->zoom*100.0);
    field(h,IDC_INFO_CURRENT,b);
    byte_size(b,256,(ULONGLONG)entry->bytes);field(h,IDC_INFO_MEMORYSIZE,b);
  } else {
    field(h,IDC_INFO_ORIGINAL,a->loading?L"読み込み中":L"取得できません");
    field(h,IDC_INFO_CURRENT,L"—");
    field(h,IDC_INFO_MEMORYSIZE,L"—");
  }
  byte_size(b,256,f->size);field(h,IDC_INFO_DISKSIZE,b);
  swprintf(b,256,L"%zu / %zu",a->files.index+1,a->files.count);field(h,IDC_INFO_POSITION,b);
  FILETIME local;
  SYSTEMTIME st;
  if(FileTimeToLocalFileTime(&f->modified,&local)&&FileTimeToSystemTime(&local,&st)) {
    swprintf(b,256,L"%04u/%02u/%02u %02u:%02u:%02u",(unsigned)st.wYear,(unsigned)st.wMonth,(unsigned)st.wDay,(unsigned)st.wHour,(unsigned)st.wMinute,(unsigned)st.wSecond);
    field(h,IDC_INFO_MODIFIED,b);
  }else field(h,IDC_INFO_MODIFIED,L"不明");
}
static INT_PTR CALLBACK info_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_INITDIALOG){populate(h,(App*)lp);return TRUE;}
  if(msg==WM_COMMAND&&(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)){
    EndDialog(h,LOWORD(wp));return TRUE;
  }
  return FALSE;
}
void app_show_image_info(App *a) {
  if(!a||!a->hwnd||!a->files.count||a->files.index>=a->files.count)return;
  DialogBoxParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_IMAGEINFO),a->hwnd,info_proc,(LPARAM)a);
}
