#include "viewer.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static App app;
static HMENU menuMain;
static HMENU popupMenu;
HMENU make_menu(void){return menuMain;}
static void show_resource_message(HWND owner, UINT textId, UINT flags){
  wchar_t buffer[512];
  if(!LoadStringW(GetModuleHandleW(NULL),textId,buffer,512))buffer[0]=L'\0';
  MessageBoxW(owner,buffer,APP_NAME,flags);
}
static void update_mode(App *a){a->cfg.startMode=a->mode==VIEW_FULLSCREEN?VIEW_NORMAL:a->mode;}
static void execute(App *a,int id){int w,h;RECT r;GetClientRect(a->hwnd,&r);w=r.right;h=r.bottom;switch(id){case IDM_OPEN:{wchar_t path[32768]=L"";OPENFILENAMEW ofn={0};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=a->hwnd;ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.lpstrFilter=L"画像ファイル\0*.jpg;*.jpeg;*.jpe;*.png;*.bmp;*.dib;*.gif;*.tif;*.tiff;*.ico;*.webp;*.heic;*.heif;*.avif\0すべてのファイル\0*.*\0";ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;if(GetOpenFileNameW(&ofn))app_open(a,path);break;}
case IDM_COPY:if(!app_copy_image_to_clipboard(a))show_resource_message(a->hwnd,IDS_CLIPBOARD_ERROR,MB_OK|MB_ICONWARNING);break;case IDM_RELOAD:app_reload(a);break;case IDM_EXIT:DestroyWindow(a->hwnd);break;case IDM_PREV:app_navigate(a,-1,FALSE);break;case IDM_NEXT:app_navigate(a,1,FALSE);break;case IDM_FIRST:app_navigate(a,-1,TRUE);break;case IDM_LAST:app_navigate(a,1,TRUE);break;case IDM_ZOOMIN:app_zoom(a,a->cfg.zoomStep,w/2,h/2);break;case IDM_ZOOMOUT:app_zoom(a,1/a->cfg.zoomStep,w/2,h/2);break;case IDM_ACTUAL:app_fit(a,FIT_ACTUAL);break;case IDM_SHRINK:app_fit(a,FIT_SHRINK);break;case IDM_FIT:app_fit(a,FIT_ALL);break;case IDM_WIDTH:app_fit(a,FIT_WIDTH);break;case IDM_HEIGHT:app_fit(a,FIT_HEIGHT);break;case IDM_LOCK:a->zoomLock=!a->zoomLock;a->cfg.zoomLock=a->zoomLock;break;case IDM_FULLSCREEN:app_toggle_mode(a,a->mode==VIEW_FULLSCREEN?VIEW_NORMAL:VIEW_FULLSCREEN);break;case IDM_SIMPLE:app_toggle_mode(a,a->mode==VIEW_SIMPLE?VIEW_NORMAL:VIEW_SIMPLE);break;case IDM_SETTINGS:app_show_settings(a);break;case IDM_ABOUT:show_resource_message(a->hwnd,IDS_ABOUT,MB_OK|MB_ICONINFORMATION);break;case IDM_SORT_NATURAL:case IDM_SORT_NAME:case IDM_SORT_DATE:case IDM_SORT_SIZE:case IDM_SORT_DESC:{if(id==IDM_SORT_DESC)a->cfg.descending=!a->cfg.descending;else a->cfg.sort=(SortMode)(id-IDM_SORT_NATURAL);app_reload(a);break;}}}

static INT_PTR CALLBACK settings_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 App *a=(App*)GetWindowLongPtrW(h,DWLP_USER);
 if(msg==WM_INITDIALOG){
  a=(App*)lp;SetWindowLongPtrW(h,DWLP_USER,(LONG_PTR)a);
  HWND c=GetDlgItem(h,IDC_FIT);
  const wchar_t *fits[]={L"大きい画像のみ縮小",L"全体に合わせる",L"原寸",L"横幅に合わせる",L"縦幅に合わせる"};
  for(int i=0;i<5;i++)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)fits[i]);
  SendMessageW(c,CB_SETCURSEL,a->cfg.initialFit,0);
  c=GetDlgItem(h,IDC_BACKGROUND);
  const wchar_t *bgs[]={L"ダークグレー (#202020)",L"ブラック (#000000)",L"市松模様"};
  for(int i=0;i<3;i++)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)bgs[i]);
  SendMessageW(c,CB_SETCURSEL,a->cfg.checkerboard?2:a->cfg.background==RGB(0,0,0)?1:0,0);
  c=GetDlgItem(h,IDC_QUALITY);
  SendMessageW(c,CB_ADDSTRING,0,(LPARAM)L"高品質 (HALFTONE)");
  SendMessageW(c,CB_ADDSTRING,0,(LPARAM)L"高速 (COLORONCOLOR)");
  SendMessageW(c,CB_SETCURSEL,a->cfg.highQuality?0:1,0);
  CheckDlgButton(h,IDC_ZOOMLOCK,a->zoomLock?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(h,IDC_LOOP,a->cfg.loop?BST_CHECKED:BST_UNCHECKED);
  return TRUE;
 }
 if(msg==WM_COMMAND){
  int id=LOWORD(wp);
  if(id==IDCANCEL){EndDialog(h,IDCANCEL);return TRUE;}
  if(id==IDOK){
   int fit=(int)SendDlgItemMessageW(h,IDC_FIT,CB_GETCURSEL,0,0);
   int bg=(int)SendDlgItemMessageW(h,IDC_BACKGROUND,CB_GETCURSEL,0,0);
   int quality=(int)SendDlgItemMessageW(h,IDC_QUALITY,CB_GETCURSEL,0,0);
   if(fit<0||fit>4||bg<0||bg>2||quality<0||quality>1)return TRUE;
   a->cfg.initialFit=(FitMode)fit;
   a->cfg.background=bg==1?RGB(0,0,0):RGB(32,32,32);
   a->cfg.checkerboard=(bg==2);
   a->cfg.highQuality=(quality==0);
   a->zoomLock=a->cfg.zoomLock=IsDlgButtonChecked(h,IDC_ZOOMLOCK)==BST_CHECKED;
   a->cfg.loop=IsDlgButtonChecked(h,IDC_LOOP)==BST_CHECKED;
   app_fit(a,a->cfg.initialFit);
   if(!settings_save(a))show_resource_message(h,IDS_SAVE_ERROR,MB_OK|MB_ICONWARNING);
   EndDialog(h,IDOK);return TRUE;
  }
 }
 return FALSE;
}
void app_show_settings(App *a){
 if(a->settingsWindow){SetForegroundWindow(a->settingsWindow);return;}
 DialogBoxParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_SETTINGS),a->hwnd,settings_proc,(LPARAM)a);
}
static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM wp,LPARAM lp){App *a=(App*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){a=((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);return TRUE;}if(!a)return DefWindowProcW(h,m,wp,lp);switch(m){case WM_CREATE:{a->hwnd=h;DragAcceptFiles(h,TRUE);a->status=CreateWindowExW(0,STATUSCLASSNAMEW,L"",WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,0,0,0,0,h,NULL,GetModuleHandleW(NULL),NULL);return 0;}
case WM_SIZE:if(a->status){ShowWindow(a->status,a->mode==VIEW_NORMAL&&a->cfg.statusbar?SW_SHOW:SW_HIDE);SendMessageW(a->status,WM_SIZE,0,0);} {RECT r;GetClientRect(h,&r);app_resize_backbuffer(a,r.right,r.bottom);if(!a->zoomLock&&a->fit!=FIT_MANUAL)app_fit(a,a->fit);}return 0;
case WM_PAINT:app_paint(a);return 0;case WM_ERASEBKGND:return 1;case WM_COMMAND:
 if(LOWORD(wp)==IDM_EXIT&&a->mode==VIEW_FULLSCREEN)execute(a,IDM_FULLSCREEN);
 else execute(a,LOWORD(wp));return 0;
case WM_MOUSEWHEEL:{int d=GET_WHEEL_DELTA_WPARAM(wp);a->wheelDelta+=d;while(a->wheelDelta>=WHEEL_DELTA){a->wheelDelta-=WHEEL_DELTA;if(GET_KEYSTATE_WPARAM(wp)&MK_CONTROL){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(h,&p);app_zoom(a,a->cfg.zoomStep,p.x,p.y);}else execute(a,IDM_PREV);}while(a->wheelDelta<=-WHEEL_DELTA){a->wheelDelta+=WHEEL_DELTA;if(GET_KEYSTATE_WPARAM(wp)&MK_CONTROL){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(h,&p);app_zoom(a,1/a->cfg.zoomStep,p.x,p.y);}else execute(a,IDM_NEXT);}return 0;}
case WM_LBUTTONDOWN:a->dragging=TRUE;a->lastMouse.x=GET_X_LPARAM(lp);a->lastMouse.y=GET_Y_LPARAM(lp);SetCapture(h);return 0;case WM_MOUSEMOVE:if(a->dragging){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};a->panX+=p.x-a->lastMouse.x;a->panY+=p.y-a->lastMouse.y;a->lastMouse=p;InvalidateRect(h,NULL,FALSE);}return 0;case WM_LBUTTONUP:if(a->dragging){a->dragging=FALSE;ReleaseCapture();}return 0;case WM_CAPTURECHANGED:a->dragging=FALSE;return 0;case WM_LBUTTONDBLCLK:execute(a,IDM_FULLSCREEN);return 0;case WM_CONTEXTMENU:{POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(p.x==-1&&p.y==-1){RECT r;GetWindowRect(h,&r);p.x=r.left+40;p.y=r.top+40;}TrackPopupMenu(popupMenu,TPM_RIGHTBUTTON,p.x,p.y,0,h,NULL);return 0;}
case WM_DROPFILES:{HDROP drop=(HDROP)wp;wchar_t path[32768];if(DragQueryFileW(drop,0,path,32768))app_open(a,path);DragFinish(drop);return 0;}
case WM_DECODED:app_received(a,(DecodeResult*)lp);return 0;case WM_DPICHANGED:{RECT *r=(RECT*)lp;SetWindowPos(h,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
case WM_CLOSE:if(a->settingsWindow){DestroyWindow(a->settingsWindow);}DestroyWindow(h);return 0;
case WM_DESTROY:{worker_stop(a);MSG q;while(PeekMessageW(&q,h,WM_DECODED,WM_DECODED,PM_REMOVE)){DecodeResult *r=(DecodeResult*)q.lParam;decoded_free(&r->image);free(r->path);free(r);}WINDOWPLACEMENT place={.length=sizeof(place)};if(a->mode!=VIEW_FULLSCREEN&&GetWindowPlacement(h,&place)){a->cfg.maximized=place.showCmd==SW_SHOWMAXIMIZED;RECT r=place.rcNormalPosition;a->cfg.x=r.left;a->cfg.y=r.top;a->cfg.width=r.right-r.left;a->cfg.height=r.bottom-r.top;}update_mode(a);settings_save(a);app_clear_cache(a);app_resize_backbuffer(a,0,0);files_close(&a->files);free(a->selectedPath);PostQuitMessage(0);return 0;}}
return DefWindowProcW(h,m,wp,lp);}
static wchar_t *command_image(void){int n=0;LPWSTR *args=CommandLineToArgvW(GetCommandLineW(),&n);wchar_t *result=n>1?dupw(args[1]):NULL;if(args)LocalFree(args);return result;}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE prev,LPWSTR cmd,int show){(void)prev;(void)cmd;SetProcessDPIAware();INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_BAR_CLASSES};InitCommonControlsEx(&ic);settings_load(&app);app.zoomLock=app.cfg.zoomLock;app.fit=app.cfg.initialFit;app.zoom=1;app.mode=VIEW_NORMAL;InitializeCriticalSection(&app.requestLock);app.event=CreateEventW(NULL,FALSE,FALSE,NULL);if(!app.event){DeleteCriticalSection(&app.requestLock);return 1;}menuMain=LoadMenuW(instance,MAKEINTRESOURCEW(IDR_MAINMENU));
 HMENU popupRoot=LoadMenuW(instance,MAKEINTRESOURCEW(IDR_POPUPMENU));
 popupMenu=popupRoot?GetSubMenu(popupRoot,0):NULL;
 HACCEL accelerators=LoadAcceleratorsW(instance,MAKEINTRESOURCEW(IDR_ACCELERATORS));
 if(!menuMain||!popupMenu||!accelerators){if(menuMain)DestroyMenu(menuMain);if(popupRoot)DestroyMenu(popupRoot);CloseHandle(app.event);DeleteCriticalSection(&app.requestLock);return 1;}
 WNDCLASSEXW wc={0};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=wndproc;wc.lpszClassName=L"LiteViewerWindow";wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_APP),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);wc.hIconSm=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_APP),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);wc.style=CS_DBLCLKS;RegisterClassExW(&wc);app.hwnd=CreateWindowExW(0,wc.lpszClassName,APP_NAME,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,app.cfg.x,app.cfg.y,app.cfg.width,app.cfg.height,NULL,menuMain,instance,&app);if(!app.hwnd){CloseHandle(app.event);DeleteCriticalSection(&app.requestLock);return 1;}app.worker=CreateThread(NULL,0,decode_worker,&app,0,NULL);if(!app.worker){show_resource_message(app.hwnd,IDS_WORKER_ERROR,MB_ICONERROR);CloseHandle(app.event);DeleteCriticalSection(&app.requestLock);DestroyWindow(app.hwnd);return 1;}ShowWindow(app.hwnd,app.cfg.maximized?SW_SHOWMAXIMIZED:show);UpdateWindow(app.hwnd);if(app.cfg.startMode==VIEW_SIMPLE)app_toggle_mode(&app,VIEW_SIMPLE);wchar_t *img=command_image();if(img){app_open(&app,img);free(img);}MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){if(!TranslateAcceleratorW(app.hwnd,accelerators,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}DestroyMenu(popupRoot);DestroyMenu(menuMain);return (int)msg.wParam;}
