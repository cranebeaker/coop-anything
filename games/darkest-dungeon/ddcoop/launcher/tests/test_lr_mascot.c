/* Isolated memory GIF decoding, timing, paint and child ownership. No Steam/game. */
#include "../src/lr_mascot.c"
#include <stdio.h>
static int failed;
#define CHECK(c) do {if(!(c)){printf("FAIL mascot line%d: %s\n",__LINE__,#c);failed++;}}while(0)
int main(int argc,char **argv) {
    /* Original two-frame1x1 synthetic GIF, no third-party/game art. */
    static const unsigned char gif[]={
        'G','I','F','8','9','a',1,0,1,0,0x80,0,0,0,0,0,255,255,255,
        0x21,0xf9,4,0,12,0,0,0,0x2c,0,0,0,0,1,0,1,0,0,2,2,0x44,1,0,
        0x21,0xf9,4,0,12,0,0,0,0x2c,0,0,0,0,1,0,1,0,0,2,2,0x4c,1,0,0x3b};
    mascot m;CHECK(!mascot_load(&m,NULL,0));CHECK(!mascot_load(&m,"bad",3));
    CHECK(mascot_load(&m,gif,sizeof gif));
    if(!m.image)return 1;
    CHECK(m.frames==2 && m.width==1 && m.height==1 && m.delay[0]==120);
    CHECK(!mascot_advance(&m,m.due-1));CHECK(mascot_advance(&m,m.due) && m.frame==1);
    CHECK(mascot_advance(&m,m.due) && m.frame==0);
    HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof info.bmiHeader;
    info.bmiHeader.biWidth=64;info.bmiHeader.biHeight=-64;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    HGDIOBJ old=SelectObject(dc,bitmap);mascot_draw(&m,dc,64,64);CHECK(pixels!=NULL);
    mascot_close(&m);CHECK(!m.image && !m.stream && !m.token);mascot_close(&m);
    if(argc>1) {
        FILE *f=fopen(argv[1],"rb");CHECK(f!=NULL);
        if(f) {
            fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);void *data=malloc((size_t)n);
            CHECK(data && fread(data,1,(size_t)n,f)==(size_t)n);fclose(f);
            CHECK(mascot_load(&m,data,(size_t)n));free(data);
            CHECK(m.frames==8 && m.width==512 && m.height==512 && m.delay[0]==120);
            mascot_draw(&m,dc,64,64);uint32_t first=0,second=0;
            for(unsigned i=0;i<64*64;i++)first=first*31+((uint32_t *)pixels)[i];
            CHECK(mascot_advance(&m,m.due));mascot_draw(&m,dc,64,64);
            for(unsigned i=0;i<64*64;i++)second=second*31+((uint32_t *)pixels)[i];
            CHECK(first!=second);mascot_close(&m);
        }
    }
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    HWND parent=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,0,0,400,400,NULL,NULL,GetModuleHandleW(NULL),NULL);
    HWND child=lr_mascot_create(GetModuleHandleW(NULL),parent,10,10,150,150);
    HRSRC embedded=FindResourceW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(201),MAKEINTRESOURCEW(10));
    CHECK(embedded ? child!=NULL : child==NULL);
    DestroyWindow(parent); /* Disposes decoded image/stream/token through child lifetime. */
    printf("Mascot GIF: synthetic frames/timing/loop/paint/cleanup and optional embedded/user image: %s (%d failures)\n",failed?"FAILED":"PASS",failed);
    return failed?1:0;
}
