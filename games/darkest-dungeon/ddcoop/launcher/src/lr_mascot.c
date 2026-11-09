#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <objidl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lr_mascot.h"

/* Flat GDI+ ABI, also usable from this C-only launcher. Definitions follow the
 * Windows SDK gdiplusflat.h/gdiplusinit.h/gdiplusimaging.h. Windows supplies DLL. */
typedef struct {UINT32 version;void *debug;BOOL suppress_thread,suppress_codecs;} mascot_startup;
typedef struct {UINT id,length;WORD type;void *value;} mascot_property;
__declspec(dllimport) int WINAPI GdiplusStartup(ULONG_PTR *,const mascot_startup *,void *);
__declspec(dllimport) void WINAPI GdiplusShutdown(ULONG_PTR);
__declspec(dllimport) int WINAPI GdipLoadImageFromStream(IStream *,void **);
__declspec(dllimport) int WINAPI GdipDisposeImage(void *);
__declspec(dllimport) int WINAPI GdipGetImageWidth(void *,UINT *);
__declspec(dllimport) int WINAPI GdipGetImageHeight(void *,UINT *);
__declspec(dllimport) int WINAPI GdipImageGetFrameCount(void *,const GUID *,UINT *);
__declspec(dllimport) int WINAPI GdipImageSelectActiveFrame(void *,const GUID *,UINT);
__declspec(dllimport) int WINAPI GdipGetPropertyItemSize(void *,UINT,UINT *);
__declspec(dllimport) int WINAPI GdipGetPropertyItem(void *,UINT,UINT,mascot_property *);
__declspec(dllimport) int WINAPI GdipCreateFromHDC(HDC,void **);
__declspec(dllimport) int WINAPI GdipDeleteGraphics(void *);
__declspec(dllimport) int WINAPI GdipDrawImageRectI(void *,void *,INT,INT,INT,INT);
static const GUID mascot_time={0x6aedbd6d,0x3fb5,0x418a,{0x83,0xa6,0x7f,0x45,0x22,0x9d,0xc8,0x72}};
enum { MASCOT_RESOURCE=201,MASCOT_MAX_FRAMES=512 };
typedef struct {
    ULONG_PTR token;
    void *image;
    IStream *stream;
    UINT frames,frame,width,height,delay[MASCOT_MAX_FRAMES];
    uint64_t due;
} mascot;
typedef struct {mascot *state;int *claimed;} mascot_create_args;
static void mascot_close(mascot *m) {
    if(m->image)GdipDisposeImage(m->image);
    if(m->stream)IStream_Release(m->stream);
    if(m->token)GdiplusShutdown(m->token);
    memset(m,0,sizeof *m);
}
static int mascot_load(mascot *m,const void *bytes,size_t size) {
    memset(m,0,sizeof *m);
    if(!bytes || size<13 || size>8*1024*1024 || (memcmp(bytes,"GIF89a",6) && memcmp(bytes,"GIF87a",6)))return 0;
    mascot_startup startup={1,0,0,0};
    if(GdiplusStartup(&m->token,&startup,NULL))return 0;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);void *p=memory?GlobalLock(memory):NULL;
    if(!p) {if(memory)GlobalFree(memory);goto fail;}
    memcpy(p,bytes,size);GlobalUnlock(memory);
    if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&m->stream))) {GlobalFree(memory);goto fail;}
    if(GdipLoadImageFromStream(m->stream,&m->image) || GdipGetImageWidth(m->image,&m->width) ||
       GdipGetImageHeight(m->image,&m->height) || !m->width || !m->height || m->width>4096 || m->height>4096 ||
       GdipImageGetFrameCount(m->image,&mascot_time,&m->frames) || !m->frames || m->frames>MASCOT_MAX_FRAMES)goto fail;
    for(UINT i=0;i<m->frames;i++)m->delay[i]=100;
    UINT property_size=0;
    if(!GdipGetPropertyItemSize(m->image,0x5100,&property_size) && property_size>=sizeof(mascot_property) && property_size<65536) {
        mascot_property *item=(mascot_property *)malloc(property_size);
        if(item) {
            if(!GdipGetPropertyItem(m->image,0x5100,property_size,item) && item->type==4 && property_size>=4*m->frames && item->length>=4*m->frames &&
               (uintptr_t)item->value>=(uintptr_t)item && (uintptr_t)item->value<=(uintptr_t)item+property_size-4*m->frames) {
                for(UINT i=0;i<m->frames;i++) {
                    uint32_t ticks=0;memcpy(&ticks,(uint8_t *)item->value+4*i,4);
                    m->delay[i]=ticks<2?20:ticks>1000?10000:ticks*10;
                }
            }
            free(item);
        }
    }
    m->due=GetTickCount64()+m->delay[0];return 1;
fail:mascot_close(m);return 0;
}
static int mascot_advance(mascot *m,uint64_t now) {
    if(!m->image || m->frames<2 || now<m->due)return 0;
    UINT next=(m->frame+1)%m->frames;
    if(GdipImageSelectActiveFrame(m->image,&mascot_time,next))return 0;
    m->frame=next;m->due=now+m->delay[next];return 1;
}
static void mascot_draw(mascot *m,HDC dc,int width,int height) {
    RECT r={0,0,width,height};FillRect(dc,&r,GetSysColorBrush(COLOR_BTNFACE));
    if(!m->image || width<=0 || height<=0)return;
    int w=width,h=MulDiv(width,m->height,m->width);
    if(h>height) {h=height;w=MulDiv(height,m->width,m->height);}
    void *graphics=NULL;
    if(!GdipCreateFromHDC(dc,&graphics)) {
        GdipDrawImageRectI(graphics,m->image,(width-w)/2,(height-h)/2,w,h);
        GdipDeleteGraphics(graphics);
    }
}
static LRESULT CALLBACK mascot_window(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    mascot *m=(mascot *)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(msg==WM_NCCREATE) {
        mascot_create_args *args=(mascot_create_args *)((CREATESTRUCTW *)lp)->lpCreateParams;
        m=args->state;*args->claimed=1;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)m);
    }
    switch(msg) {
    case WM_CREATE:SetTimer(h,1,m->delay[0],NULL);return 0;
    case WM_TIMER:
        if(m && IsWindowVisible(h) && !IsIconic(GetAncestor(h,GA_ROOT)) && mascot_advance(m,GetTickCount64())) {
            InvalidateRect(h,NULL,FALSE);SetTimer(h,1,m->delay[m->frame],NULL);
        }
        return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PRINTCLIENT: {
        RECT r;GetClientRect(h,&r);mascot_draw(m,(HDC)wp,r.right,r.bottom);return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;RECT r;HDC dc=BeginPaint(h,&ps);GetClientRect(h,&r);
        HDC back=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);
        if(back && bitmap) {
            HGDIOBJ old=SelectObject(back,bitmap);mascot_draw(m,back,r.right,r.bottom);
            BitBlt(dc,0,0,r.right,r.bottom,back,0,0,SRCCOPY);SelectObject(back,old);
        } else mascot_draw(m,dc,r.right,r.bottom);
        if(bitmap)DeleteObject(bitmap);if(back)DeleteDC(back);EndPaint(h,&ps);return 0;
    }
    case WM_NCDESTROY:
        KillTimer(h,1);if(m) {mascot_close(m);free(m);SetWindowLongPtrW(h,GWLP_USERDATA,0);}break;
    }
    (void)wp;return DefWindowProcW(h,msg,wp,lp);
}
HWND lr_mascot_create(HINSTANCE instance,HWND parent,int x,int y,int width,int height) {
    HRSRC resource=FindResourceW(instance,MAKEINTRESOURCEW(MASCOT_RESOURCE),MAKEINTRESOURCEW(10));
    if(!resource)return NULL;
    HGLOBAL loaded=LoadResource(instance,resource);
    mascot *m=(mascot *)calloc(1,sizeof *m);
    if(!m)return NULL;
    if(!mascot_load(m,LockResource(loaded),SizeofResource(instance,resource))) {free(m);return NULL;}
    WNDCLASSW cls={0};cls.lpfnWndProc=mascot_window;cls.hInstance=instance;cls.lpszClassName=L"DDCoopMascot";
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {mascot_close(m);free(m);return NULL;}
    int claimed=0;mascot_create_args args={m,&claimed};
    HWND h=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,x,y,width,height,parent,NULL,instance,&args);
    if(!h && !claimed) {mascot_close(m);free(m);}
    return h;
}
