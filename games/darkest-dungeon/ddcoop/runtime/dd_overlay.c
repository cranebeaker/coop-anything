/* Small SDL/OpenGL panel. GDI rasterizes text into our own bitmap; OpenGL draws that bitmap just
 * before the game's swap. No game UI objects or graphics state are retained across a frame. */
#define WIN32_LEAN_AND_MEAN
#include "dd_overlay.h"
#include "dd_overlay_wire.h"
#include "dd_log.h"
#include <windows.h> /* The Windows SDK requires this before GL/gl.h. */
#include <GL/gl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    PANEL_W = 370,
    PANEL_H = 650,
    KEYDOWN = 0x300,
    KEYUP = 0x301,
    MOTION = 0x400,
    BUTTONDOWN = 0x401,
    BUTTONUP = 0x402,
    WHEEL = 0x403,
    NOTICES = 4,
    NOTICE_LENGTH = 512
};
typedef void (*size_fn)(void *, int *, int *);
static size_fn g_overlay_window_size, g_overlay_drawable_size;
static struct {
    int visible, section, scroll[3], mouse_x, mouse_y, pressed, width, height, draw_w, draw_h;
    float scale;
    uint64_t last_read;
    HANDLE mapping;
    const dd_overlay_snapshot *view;
    dd_overlay_snapshot snapshot;
    UINT message;
    int fresh;
    HDC dc;
    HBITMAP bitmap;
    uint8_t *pixels;
    HFONT font, bold;
    HGLRC context;
    GLuint program, vao, texture;
    GLint bounds, tex;
    int gl_failed;
    char notices[NOTICES][NOTICE_LENGTH]; /* Oldest first; the panel shows the first. */
    int notice_count, notice_opened;      /* notice_opened: a notice opened the panel, so the last one closes it. */
    int session;                          /* DD_OVERLAY_*, from the runtime. */
    int turn;                             /* DD_OVERLAY_TURN_*, from the runtime. */
    int party_count, party_owner[4];      /* dd_overlay_party */
    float party_stress[4];
} O;

/* Function pointers deliberately use the Windows OpenGL calling convention. */
#define PROC(ret, name, args)                                                                                \
    typedef ret(APIENTRY *name##_fn) args;                                                                   \
    static name##_fn p_##name
PROC(GLuint, CreateShader, (GLenum));
PROC(void, ShaderSource, (GLuint, GLsizei, const char *const *, const GLint *));
PROC(void, CompileShader, (GLuint));
PROC(void, GetShaderiv, (GLuint, GLenum, GLint *));
PROC(void, DeleteShader, (GLuint));
PROC(GLuint, CreateProgram, (void));
PROC(void, AttachShader, (GLuint, GLuint));
PROC(void, LinkProgram, (GLuint));
PROC(void, GetProgramiv, (GLuint, GLenum, GLint *));
PROC(void, DeleteProgram, (GLuint));
PROC(void, UseProgram, (GLuint));
PROC(GLint, GetUniformLocation, (GLuint, const char *));
PROC(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat));
PROC(void, Uniform1i, (GLint, GLint));
PROC(void, GenVertexArrays, (GLsizei, GLuint *));
PROC(void, BindVertexArray, (GLuint));
PROC(void, ActiveTexture, (GLenum));
PROC(void, BindBuffer, (GLenum, GLuint));
PROC(void, BindFramebuffer, (GLenum, GLuint));
PROC(void, BlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum));
PROC(void, BlendEquationSeparate, (GLenum, GLenum));
PROC(void, BindSampler, (GLuint, GLuint));
#undef PROC
static int gl_load(void) {
#define LOAD(name)                                                                                           \
    do {                                                                                                     \
        PROC pointer = wglGetProcAddress("gl" #name);                                                        \
        memcpy(&p_##name, &pointer, sizeof pointer);                                                         \
        if (!pointer || (uintptr_t)pointer <= 3 || (intptr_t)pointer == -1) return 0;                        \
    } while (0)
    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(DeleteShader);
    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(LinkProgram);
    LOAD(GetProgramiv);
    LOAD(DeleteProgram);
    LOAD(UseProgram);
    LOAD(GetUniformLocation);
    LOAD(Uniform4f);
    LOAD(Uniform1i);
    LOAD(GenVertexArrays);
    LOAD(BindVertexArray);
    LOAD(ActiveTexture);
    LOAD(BindBuffer);
    LOAD(BindFramebuffer);
    LOAD(BlendFuncSeparate);
    LOAD(BlendEquationSeparate);
    {
        PROC pointer = wglGetProcAddress("glBindSampler");
        memcpy(&p_BindSampler, &pointer, sizeof pointer);
    }
#undef LOAD
    return 1;
}
static GLuint shader(GLenum type, const char *source) {
    GLuint id = p_CreateShader(type);
    GLint ok = 0;
    if (!id) return 0;
    p_ShaderSource(id, 1, &source, NULL);
    p_CompileShader(id);
    p_GetShaderiv(id, 0x8b81, &ok);
    if (!ok) {
        p_DeleteShader(id);
        return 0;
    }
    return id;
}
static int gl_initialize(void) {
    if (!gl_load()) return 0;
    const char *vertex = "#version 130\nuniform vec4 bounds;out vec2 uv;void main(){vec2 "
                         "p=vec2((gl_VertexID==1||gl_VertexID==2||gl_VertexID==4)?1.0:0.0,(gl_VertexID>=2&&"
                         "gl_VertexID<=4)?1.0:0.0);uv=p;gl_Position=vec4(bounds.xy+p*bounds.zw,0,1);}";
    const char *fragment = "#version 130\nuniform sampler2D image;in vec2 uv;out vec4 color;void "
                           "main(){color=texture(image,uv);}";
    GLuint v = shader(0x8b31, vertex), f = shader(0x8b30, fragment);
    GLint ok = 0;
    if (!v || !f) {
        if (v) p_DeleteShader(v);
        if (f) p_DeleteShader(f);
        return 0;
    }
    O.program = p_CreateProgram();
    p_AttachShader(O.program, v);
    p_AttachShader(O.program, f);
    p_LinkProgram(O.program);
    p_GetProgramiv(O.program, 0x8b82, &ok);
    p_DeleteShader(v);
    p_DeleteShader(f);
    if (!ok) {
        p_DeleteProgram(O.program);
        O.program = 0;
        return 0;
    }
    O.bounds = p_GetUniformLocation(O.program, "bounds");
    O.tex = p_GetUniformLocation(O.program, "image");
    p_GenVertexArrays(1, &O.vao);
    glGenTextures(1, &O.texture);
    return O.vao && O.texture;
}
static void read_snapshot(void) {
    uint64_t now = GetTickCount64();
    if (now - O.last_read < 100) return;
    O.last_read = now;
    if (!O.view) {
        wchar_t name[96];
        swprintf(name, 96, L"Local\\DDCoopOverlay-%lu", GetCurrentProcessId());
        O.mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
        if (O.mapping) O.view = MapViewOfFile(O.mapping, FILE_MAP_READ, 0, 0, sizeof(dd_overlay_snapshot));
        if (O.mapping && !O.view) {
            CloseHandle(O.mapping);
            O.mapping = NULL;
        }
    }
    O.fresh = 0;
    if (!O.view) return;
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        LONG before = O.view->sequence;
        if (before & 1) continue;
        MemoryBarrier();
        dd_overlay_snapshot next;
        memcpy(&next, O.view, sizeof next);
        MemoryBarrier();
        if (before != O.view->sequence || next.magic != DD_OVERLAY_MAGIC ||
            next.version != DD_OVERLAY_VERSION || next.size != sizeof next ||
            next.hero_count > DD_OVERLAY_HEROES || now < next.updated || now - next.updated > 5000)
            continue;
        next.role[sizeof next.role - 1] = 0;
        next.status[sizeof next.status - 1] = 0;
        next.runtime[sizeof next.runtime - 1] = 0;
        for (unsigned i = 0; i < DD_OVERLAY_CHECKS; i++)
            next.checks[i][sizeof next.checks[i] - 1] = 0;
        for (unsigned i = 0; i < next.hero_count; i++) {
            next.heroes[i].name[sizeof next.heroes[i].name - 1] = 0;
            next.heroes[i].cls[sizeof next.heroes[i].cls - 1] = 0;
            next.heroes[i].owner[sizeof next.heroes[i].owner - 1] = 0;
        }
        O.snapshot = next;
        O.fresh = 1;
        break;
    }
}
static void command(unsigned action, uint32_t value) {
    if (!O.fresh) return;
    HWND launcher = (HWND)(uintptr_t)O.snapshot.launcher_window;
    DWORD pid = 0;
    if (!IsWindow(launcher) || !GetWindowThreadProcessId(launcher, &pid) || pid != O.snapshot.launcher_pid)
        return;
    if (!O.message) O.message = RegisterWindowMessageW(DD_OVERLAY_MESSAGE);
    if (O.message) PostMessageW(launcher, O.message, action, (LPARAM)value);
}
static int inside(void) {
    float scale = O.scale > 0 ? O.scale : 1;
    return O.mouse_x >= 12 && O.mouse_y >= 12 && O.mouse_x < 12 + PANEL_W * scale &&
           O.mouse_y < 12 + O.height * scale;
}
static int content_rows(void) { return (O.height - 205) / 48; }
static int check_height(const char *value) {
    if (!O.dc || !O.font) return 90;
    wchar_t wide[1024] = {0};
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wide, 1024);
    RECT r = {0, 0, 310, 0};
    SelectObject(O.dc, O.font);
    DrawTextW(O.dc, wide, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    return r.bottom > 30 ? r.bottom + 14 : 44;
}
static void scroll_clamp(void) {
    int max = 0;
    if (O.section == 1) max = (int)O.snapshot.hero_count - content_rows();
    if (O.section == 2) {
        for (unsigned i = 0; i < DD_OVERLAY_CHECKS; i++)
            max += check_height(O.snapshot.checks[i]);
        max -= O.height - 205;
    }
    if (max < 0) max = 0;
    if (O.scroll[O.section] < 0) O.scroll[O.section] = 0;
    if (O.scroll[O.section] > max) O.scroll[O.section] = max;
}
static int notice_ok_y(void) { return O.height - 80 < 340 ? O.height - 80 : 340; }
static void notice_remove(int index) {
    for (int i = index; i + 1 < O.notice_count; i++) memcpy(O.notices[i], O.notices[i + 1], NOTICE_LENGTH);
    O.notice_count--;
    if (!O.notice_count && O.notice_opened) O.visible = 0;
    if (!O.notice_count) O.notice_opened = 0;
}
void dd_overlay_session(int state) { O.session = state; }
void dd_overlay_turn(int turn) { O.turn = turn; }
void dd_overlay_party(int count, const int owner[4], const float stress[4]) {
    if (count < 0 || count > 4 || !owner || !stress) count = 0;
    O.party_count = count;
    for (int i = 0; i < count; i++) {
        O.party_owner[i] = owner[i];
        O.party_stress[i] = stress[i] != stress[i] ? 0.0f : stress[i] < 0 ? 0.0f : stress[i] > 200.0f ? 200.0f : stress[i];
    }
}
/* A cleared rectangle, nothing else: no shader, texture or panel needed, and all of it put back. */
static void turn_strip(void) {
    GLint box[4];
    GLfloat clear[4];
    GLboolean mask[4], scissor = glIsEnabled(GL_SCISSOR_TEST);
    int height = O.draw_h >= 900 ? 12 : 8;
    glGetIntegerv(GL_SCISSOR_BOX, box);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, O.draw_h - height, O.draw_w, height);
    if (O.turn == DD_OVERLAY_TURN_MINE) glClearColor(0.30f, 0.80f, 0.45f, 1.0f);
    else glClearColor(0.92f, 0.55f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    glScissor(box[0], box[1], box[2], box[3]);
    if (!scissor) glDisable(GL_SCISSOR_TEST);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
}
/* Where block i (0 = the front rank) goes, in GL pixels (origin bottom left). */
static void party_box(int i, int *x, int *y, int *w, int *h) {
    int strip = O.draw_h >= 900 ? 12 : 8;
    *w = O.draw_w / 24 < 40 ? 40 : O.draw_w / 24;
    *h = *w / 3;
    *x = 12 + (O.party_count - 1 - i) * (*w + 6);
    *y = O.draw_h - strip - 6 - *h;
}
static void party_clear(int x, int y, int w, int h, float r, float g, float b) {
    if (w <= 0 || h <= 0) return;
    glScissor(x, y, w, h);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
/* Cleared rectangles only, like the turn strip, and all state put back. */
static void party_marks(void) {
    GLint box[4];
    GLfloat clear[4];
    GLboolean mask[4], scissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, box);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < O.party_count; i++) {
        int x, y, w, h, bar;
        party_box(i, &x, &y, &w, &h);
        bar = h / 3 < 3 ? 3 : h / 3;
        if (O.party_owner[i] == DD_OVERLAY_TURN_MINE) party_clear(x, y, w, h, 0.30f, 0.80f, 0.45f);
        else if (O.party_owner[i] == DD_OVERLAY_TURN_OTHER) party_clear(x, y, w, h, 0.92f, 0.55f, 0.20f);
        else party_clear(x, y, w, h, 0.45f, 0.45f, 0.45f);
        party_clear(x + 2, y + 2, w - 4, bar, 0.08f, 0.08f, 0.08f);
        party_clear(x + 2, y + 2, (int)((w - 4) * O.party_stress[i] / 200.0f), bar, 0.95f, 0.95f, 0.95f);
    }
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    glScissor(box[0], box[1], box[2], box[3]);
    if (!scissor) glDisable(GL_SCISSOR_TEST);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
}
static const char *session_text(void) {
    switch (O.session) {
    case DD_OVERLAY_CONNECTED: return "Spelers verbonden";
    case DD_OVERLAY_LOST: return "Verbroken: sluit het spel, start opnieuw";
    case DD_OVERLAY_REFUSED: return "Host antwoordt niet";
    case DD_OVERLAY_WAITING: return "Wacht op de andere speler";
    default: return "Geen gedeelde verbinding";
    }
}
/* The connection comes from the runtime, so it is known without a launcher too: a window started by hand has none. */
static const char *status_text(void) { return O.fresh ? O.snapshot.status : "Geen launcher bij dit venster. Mis je die? Zie de taakbalk."; }
static const char *sync_text(void) {
    if (O.snapshot.runtime[0]) return O.snapshot.runtime;
    return O.session == DD_OVERLAY_CONNECTED ? "De gedeelde sessie loopt." : "Wacht tot de co-op campagne geladen is.";
}
int dd_overlay_notice(const char *utf8) {
    if (O.gl_failed || !utf8 || !*utf8) return 0;
    for (int i = 0; i < O.notice_count; i++)
        if (!strcmp(O.notices[i], utf8)) {
            if (!O.visible) {
                O.visible = 1;
                O.notice_opened = 1;
            }
            return 1;
        }
    if (O.notice_count == NOTICES) notice_remove(0); /* The log keeps every message. */
    snprintf(O.notices[O.notice_count++], NOTICE_LENGTH, "%s", utf8);
    if (!O.visible) {
        O.visible = 1;
        O.notice_opened = 1;
    }
    return 1;
}
void dd_overlay_notice_clear(const char *utf8) {
    for (int i = O.notice_count - 1; utf8 && i >= 0; i--)
        if (!strcmp(O.notices[i], utf8)) notice_remove(i);
}
static void click(void) {
    float scale = O.scale > 0 ? O.scale : 1;
    int x = (int)((O.mouse_x - 12) / scale), y = (int)((O.mouse_y - 12) / scale);
    if (y < 30 && x > PANEL_W - 35) {
        O.visible = 0;
        return;
    }
    if (O.notice_count && y >= 95 && y < O.height - 35) {
        if (y >= notice_ok_y() && y < notice_ok_y() + 34) notice_remove(0);
        return;
    }
    if (y >= 95 && y < 125) {
        O.section = x < 123 ? 0 : x < 246 ? 1 : 2;
        scroll_clamp();
        return;
    }
    if (y >= O.height - 35) {
        command(x < 120 ? DD_OVERLAY_LOGS : x < 240 ? DD_OVERLAY_COPY_CODE : DD_OVERLAY_LAUNCHER, 0);
        return;
    }
    if (O.section == 1 && y >= 140 && y < O.height - 65) {
        int row = O.scroll[1] + (y - 140) / 48;
        if (row >= 0 && (unsigned)row < O.snapshot.hero_count) {
            const dd_overlay_hero *h = &O.snapshot.heroes[row];
            command(h->mine ? DD_OVERLAY_RELEASE : DD_OVERLAY_CLAIM, h->id);
        }
    }
}
int dd_overlay_event(const void *event) {
    if (!event) return 0;
    uint32_t type = 0;
    const uint8_t *e = event;
    memcpy(&type, e, 4);
    if (type == KEYDOWN || type == KEYUP) {
        int32_t key = 0;
        memcpy(&key, e + 20, 4);
        if (key == 1073741890) {
            if (type == KEYDOWN && !e[13]) {
                O.visible = !O.visible;
                O.notice_opened = 0;
                dd_log("F9 panel %s", O.visible ? "shown" : "hidden");
            }
            return 1;
        }
        if (O.visible && key == 27) {
            if (type == KEYDOWN) O.visible = 0;
            return 1;
        }
        return O.visible;
    }
    if (type == 0x200 && e[12] == 13) {
        O.visible = 0;
        O.pressed = 0;
        return 0;
    } /* focus lost */
    if (type == 0x200 && e[12] == 12 && O.notice_count) {
        O.visible = 1;
        return 0;
    } /* focus back with an unread message */
    if (type == BUTTONUP && O.pressed) { /* Also when that click closed the panel: the game never saw its press. */
        O.pressed = 0;
        return 1;
    }
    if (!O.visible) return 0;
    if (type == MOTION || type == BUTTONDOWN || type == BUTTONUP) {
        memcpy(&O.mouse_x, e + 20, 4);
        memcpy(&O.mouse_y, e + 24, 4);
        if (type == BUTTONDOWN && inside()) {
            O.pressed = 1;
            if (e[16] == 1) click();
            return 1;
        }
        return inside() || O.pressed;
    }
    if (type == WHEEL && inside()) {
        int32_t amount = 0;
        uint32_t direction = 0;
        memcpy(&amount, e + 20, 4);
        memcpy(&direction, e + 24, 4);
        if (amount > 5) amount = 5;
        if (amount < -5) amount = -5;
        if (direction == 1) amount = -amount;
        O.scroll[O.section] -= amount * (O.section == 2 ? 30 : 1);
        scroll_clamp();
        return 1;
    }
    return 0;
}
static void fill(int x, int y, int w, int h, COLORREF color) {
    RECT r = {x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(color);
    FillRect(O.dc, &r, b);
    DeleteObject(b);
}
static void text(int x, int y, int w, int h, const char *value, COLORREF color, int bold) {
    wchar_t wide[1024] = {0};
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wide, 1024);
    wide[1023] = 0;
    RECT r = {x, y, x + w, y + h};
    SetTextColor(O.dc, color);
    SelectObject(O.dc, bold ? O.bold : O.font);
    DrawTextW(O.dc, wide, -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
}
static int bitmap_initialize(void) {
    if (O.dc) return 1;
    O.dc = CreateCompatibleDC(NULL);
    if (!O.dc) return 0;
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = PANEL_W;
    info.bmiHeader.biHeight = -PANEL_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    O.bitmap = CreateDIBSection(O.dc, &info, DIB_RGB_COLORS, (void **)&O.pixels, NULL, 0);
    if (!O.bitmap || !O.pixels) return 0;
    SelectObject(O.dc, O.bitmap);
    SetBkMode(O.dc, TRANSPARENT);
    O.font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                         L"Segoe UI");
    O.bold = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                         L"Segoe UI");
    return O.font && O.bold;
}
static void raster(void) {
    COLORREF white = RGB(225, 226, 229), muted = RGB(152, 160, 172), gold = RGB(234, 193, 115);
    fill(0, 0, PANEL_W, O.height, RGB(17, 20, 25));
    fill(0, 0, PANEL_W, 31, RGB(33, 40, 50));
    text(12, 6, 320, 24, "DDCoop   |   F9 verbergen", white, 1);
    text(PANEL_W - 25, 5, 20, 24, "x", muted, 1);
    text(12, 40, 65, 22, O.snapshot.role[0] ? O.snapshot.role : "DDCoop", gold, 1);
    int connected = O.session == DD_OVERLAY_CONNECTED;
    text(80, 40, 280, 22, session_text(),
         connected ? RGB(119, 205, 149) : RGB(234, 157, 111), 0);
    text(12, 64, 345, 26, status_text(), white, 0);
    if (O.notice_count) {
        char title[32];
        int ok = notice_ok_y();
        if (O.notice_count > 1)
            snprintf(title, sizeof title, "Melding (nog %d)", O.notice_count - 1);
        else
            snprintf(title, sizeof title, "Melding");
        fill(4, 95, PANEL_W - 8, O.height - 135, RGB(58, 45, 24));
        text(14, 104, 340, 24, title, gold, 1);
        text(14, 132, 340, ok - 140, O.notices[0], white, 0);
        fill(14, ok, PANEL_W - 28, 34, RGB(52, 78, 107));
        text(PANEL_W / 2 - 10, ok + 7, 60, 24, "OK", white, 1);
        goto footer;
    }
    const char *sections[] = {"Verbinding", "Helden", "Controles"};
    for (int i = 0; i < 3; i++) {
        fill(i * 123 + 4, 95, 119, 30, O.section == i ? RGB(52, 78, 107) : RGB(34, 43, 56));
        text(i * 123 + 14, 101, 102, 22, sections[i], white, 1);
    }
    if (O.section == 0) {
        text(12, 144, 345, 28, "Verbinding en synchronisatie", gold, 1);
        text(12, 178, 345, 96, sync_text(), white, 0);
        text(12, 290, 345, 80,
             "De host bewaart de voortgang. Bij de gast wordt de eigen campagne na afloop automatisch "
             "teruggezet.",
             muted, 0);
        text(12, 385, 345, 100,
             "Host/Join en uitnodigen blijven beschikbaar in de launcher. Hier kun je tijdens het spelen "
             "helden toewijzen en controles bekijken.",
             muted, 0);
    } else if (O.section == 1) {
        int rows = content_rows();
        for (int i = 0; i < rows; i++) {
            int index = i + O.scroll[1];
            if ((unsigned)index >= O.snapshot.hero_count) break;
            const dd_overlay_hero *h = &O.snapshot.heroes[index];
            int y = 140 + i * 48;
            fill(7, y - 2, PANEL_W - 19, 45, h->party ? RGB(66, 52, 31) : RGB(29, 35, 44));
            text(14, y + 5, 24, 24, h->mine ? "[x]" : "[ ]", h->mine ? RGB(119, 205, 149) : muted, 1);
            text(45, y, 240, 22, h->name, h->party ? gold : white, 1);
            text(45, y + 23, 220, 20, h->cls, muted, 0);
            text(285, y + 7, 65, 24, h->owner, white, 0);
        }
        text(12, O.height - 63, 345, 24, "Klik: jij bestuurt deze held. Party staat bovenaan.", muted, 0);
    } else {
        int saved = SaveDC(O.dc), y = 140 - O.scroll[2];
        IntersectClipRect(O.dc, 0, 136, PANEL_W, O.height - 68);
        for (int index = 0; index < DD_OVERLAY_CHECKS; index++) {
            int height = check_height(O.snapshot.checks[index]);
            text(12, y, 24, 24,
                 O.snapshot.check_ok[index] > 0   ? "+"
                 : O.snapshot.check_ok[index] < 0 ? "!"
                                                  : "-",
                 O.snapshot.check_ok[index] < 0 ? RGB(234, 157, 111) : RGB(119, 205, 149), 1);
            text(38, y, 310, height - 8, O.snapshot.checks[index], white, 0);
            y += height;
        }
        RestoreDC(O.dc, saved);
        text(12, O.height - 63, 345, 24, "Muiswiel: verder scrollen", muted, 0);
    }
footer:
    fill(4, O.height - 36, PANEL_W - 8, 32, RGB(34, 43, 56));
    text(18, O.height - 29, 100, 24, "Logs kopieren", white, 0);
    text(135, O.height - 29, 100, 24, "Kopieer code", white, 0);
    text(263, O.height - 29, 98, 24, "Launcher", white, 0);
    GdiFlush();
    for (int i = 0; i < PANEL_W * O.height; i++)
        O.pixels[i * 4 + 3] = 245;
}

void dd_overlay_draw(void *window) {
    if ((!O.visible && !O.turn && !O.party_count) || !window) return;
    read_snapshot();
    if (!g_overlay_window_size) {
        HMODULE sdl = GetModuleHandleW(L"SDL2_orig.dll");
        if (sdl) {
            FARPROC a = GetProcAddress(sdl, "SDL_GetWindowSize"),
                    b = GetProcAddress(sdl, "SDL_GL_GetDrawableSize");
            memcpy(&g_overlay_window_size, &a, sizeof a);
            memcpy(&g_overlay_drawable_size, &b, sizeof b);
        }
    }
    int w = 0, h = 0;
    if (!g_overlay_window_size || !g_overlay_drawable_size) return;
    g_overlay_window_size(window, &w, &h);
    g_overlay_drawable_size(window, &O.draw_w, &O.draw_h);
    if (w < 400 || h < 280 || O.draw_w <= 0 || O.draw_h <= 0) return;
    if (O.turn && wglGetCurrentContext()) turn_strip();
    if (O.party_count && wglGetCurrentContext()) party_marks();
    if (!O.visible) return;
    O.width = w;
    O.scale = w >= 1800 ? 1.25f : 1.0f;
    O.height = (int)((h - 24) / O.scale);
    if (O.height > PANEL_H) O.height = PANEL_H;
    if (O.height < 280) return;
    if (!bitmap_initialize()) return;
    scroll_clamp();
    raster();
    HGLRC context = wglGetCurrentContext();
    if (!context) return;
    if (context != O.context) {
        O.context = context;
        O.program = O.vao = O.texture = 0;
        O.gl_failed = 0;
    }
    if (O.gl_failed) return;
    /* Initialization only creates our resources and restores the bindings below along with drawing. */
    int first_draw = !O.program;
    if (first_draw && !gl_initialize()) {
        O.gl_failed = 1;
        OutputDebugStringW(L"DDCoop F9: OpenGL panel could not initialize");
        dd_log("F9 panel OpenGL initialization failed; use launcher taskbar for logs");
        return;
    }
    if (first_draw) dd_log("F9 panel OpenGL ready (drawable %dx%d)", O.draw_w, O.draw_h);
    GLint program = 0, vao = 0, active = 0, texture = 0, sampler = 0, framebuffer = 0, unpack = 0,
          alignment = 0, rowlen = 0, skiprows = 0, skippixels = 0;
    GLint viewport[4], blend_src = 0, blend_dst = 0, alpha_src = 0, alpha_dst = 0, equation = 0,
                       alpha_equation = 0, polygon[2];
    GLboolean mask[4];
    const GLenum caps[] = {GL_BLEND,        GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST,
                           GL_STENCIL_TEST, 0x8c89,        0x8db9};
    GLboolean enabled[7];
    glGetIntegerv(0x8b8d, &program);
    glGetIntegerv(0x85b5, &vao);
    glGetIntegerv(0x84e0, &active);
    p_ActiveTexture(0x84c0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    if (p_BindSampler) glGetIntegerv(0x8919, &sampler);
    glGetIntegerv(0x8ca6, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(0x88ef, &unpack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlen);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippixels);
    glGetIntegerv(0x80c9, &blend_src);
    glGetIntegerv(0x80c8, &blend_dst);
    glGetIntegerv(0x80cb, &alpha_src);
    glGetIntegerv(0x80ca, &alpha_dst);
    glGetIntegerv(0x8009, &equation);
    glGetIntegerv(0x883d, &alpha_equation);
    glGetIntegerv(GL_POLYGON_MODE, polygon);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    for (unsigned i = 0; i < 7; i++) {
        enabled[i] = glIsEnabled(caps[i]);
        glDisable(caps[i]);
    }
    glEnable(GL_BLEND);
    p_BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    p_BlendEquationSeparate(0x8006, 0x8006);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    p_BindFramebuffer(0x8ca9, 0);
    glViewport(0, 0, O.draw_w, O.draw_h);
    p_BindBuffer(0x88ec, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    if (p_BindSampler) p_BindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, O.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PANEL_W, O.height, 0, 0x80e1, GL_UNSIGNED_BYTE, O.pixels);
    p_UseProgram(O.program);
    p_Uniform1i(O.tex, 0);
    p_BindVertexArray(O.vao);
    p_Uniform4f(O.bounds, -1.0f + 24.0f / w, 1.0f - 24.0f / h, 2.0f * PANEL_W * O.scale / w,
                -2.0f * O.height * O.scale / h);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    p_BindVertexArray((GLuint)vao);
    p_UseProgram((GLuint)program);
    glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
    if (p_BindSampler) p_BindSampler(0, (GLuint)sampler);
    p_ActiveTexture((GLenum)active);
    p_BindBuffer(0x88ec, (GLuint)unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippixels);
    p_BindFramebuffer(0x8ca9, (GLuint)framebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    p_BlendFuncSeparate((GLenum)blend_src, (GLenum)blend_dst, (GLenum)alpha_src, (GLenum)alpha_dst);
    p_BlendEquationSeparate((GLenum)equation, (GLenum)alpha_equation);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)polygon[0]);
    for (unsigned i = 0; i < 7; i++)
        if (enabled[i])
            glEnable(caps[i]);
        else
            glDisable(caps[i]);
}
