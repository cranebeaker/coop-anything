/* Own hidden Win32/GL context + fictitious launcher mapping only. Never opens Steam/game/saves. */
#include "../runtime/dd_overlay.c"
static int failures, commands;
static WPARAM command_kind;
static LPARAM command_value;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #x);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static LRESULT CALLBACK fixture_proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (O.message && m == O.message) {
        commands++;
        command_kind = a;
        command_value = b;
        return 0;
    }
    return DefWindowProcW(w, m, a, b);
}
static void fixture_size(void *window, int *w, int *h) {
    (void)window;
    *w = 1280;
    *h = 720;
}
static int event(uint32_t kind, int x, int y, int key) {
    uint8_t e[56] = {0};
    memcpy(e, &kind, 4);
    if (kind == KEYDOWN || kind == KEYUP)
        memcpy(e + 20, &key, 4);
    else {
        memcpy(e + 20, &x, 4);
        memcpy(e + 24, &y, 4);
        e[16] = 1;
    }
    return dd_overlay_event(e);
}
static void save_bitmap(const char *path) {
    BITMAPFILEHEADER file = {0};
    BITMAPINFOHEADER info = {0};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof file + sizeof info;
    info.biSize = sizeof info;
    info.biWidth = PANEL_W;
    info.biHeight = -O.height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    file.bfSize = file.bfOffBits + PANEL_W * O.height * 4;
    FILE *out = fopen(path, "wb");
    CHECK(out != NULL);
    if (!out) return;
    fwrite(&file, sizeof file, 1, out);
    fwrite(&info, sizeof info, 1, out);
    fwrite(O.pixels, PANEL_W * O.height * 4, 1, out);
    fclose(out);
}
int main(int argc, char **argv) {
    WNDCLASSW cls = {0};
    cls.style = CS_OWNDC;
    cls.lpfnWndProc = fixture_proc;
    cls.lpszClassName = L"DDCoopOverlayFixture";
    RegisterClassW(&cls);
    HWND window = CreateWindowW(cls.lpszClassName, L"DDCoop isolated overlay fixture", WS_POPUP, 0,
                                0, 1280, 720, NULL, NULL, NULL, NULL);
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pixel = {0};
    pixel.nSize = sizeof pixel;
    pixel.nVersion = 1;
    pixel.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel.iPixelType = PFD_TYPE_RGBA;
    pixel.cColorBits = 32;
    int format = ChoosePixelFormat(dc, &pixel);
    CHECK(format && SetPixelFormat(dc, format, &pixel));
    HGLRC context = wglCreateContext(dc);
    CHECK(context && wglMakeCurrent(dc, context));
    printf("Overlay fixture GL: %s\n", glGetString(GL_VERSION));
    wchar_t mapping_name[96];
    swprintf(mapping_name, 96, L"Local\\DDCoopOverlay-%lu", GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                        sizeof(dd_overlay_snapshot), mapping_name);
    dd_overlay_snapshot *s = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof *s);
    CHECK(s != NULL);
    if (!s) return 1;
    memset(s, 0, sizeof *s);
    s->magic = DD_OVERLAY_MAGIC;
    s->version = DD_OVERLAY_VERSION;
    s->size = sizeof *s;
    s->updated = GetTickCount64();
    s->launcher_pid = GetCurrentProcessId();
    s->launcher_window = (uintptr_t)window;
    strcpy(s->role, "GAST");
    strcpy(s->status, "Verbonden met de host. Hamlet gereed.");
    strcpy(s->runtime, "Sessie verbonden. Wijzigingen worden door de host gecontroleerd.");
    s->hero_count = 12;
    for (unsigned i = 0; i < 12; i++) {
        s->heroes[i].id = i + 40;
        s->heroes[i].party = i < 4;
        s->heroes[i].mine = i % 2;
        snprintf(s->heroes[i].name, 64, "Hero %u", i + 1);
        strcpy(s->heroes[i].cls, i % 2 ? "vestal" : "crusader");
        strcpy(s->heroes[i].owner, i % 2 ? "jij" : "host");
    }
    for (unsigned i = 0; i < DD_OVERLAY_CHECKS; i++) {
        s->check_ok[i] = 1;
        snprintf(s->checks[i], 200,
                 "Controle %u: volledige controletekst blijft leesbaar in het scrollbare paneel.", i + 1);
    }
    g_overlay_window_size = g_overlay_drawable_size = fixture_size;
    CHECK(event(KEYDOWN, 0, 0, 1073741890) && O.visible);
    CHECK(event(KEYUP, 0, 0, 1073741890));
    glClearColor(0.05f,0.08f,0.12f,1.0f);glClear(GL_COLOR_BUFFER_BIT);
    glViewport(11, 17, 400, 500);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    dd_overlay_draw((void *)1);
    CHECK(!O.gl_failed && O.program && O.fresh);
    GLint viewport[4];
    GLboolean mask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    CHECK(viewport[0] == 11 && viewport[1] == 17 && viewport[2] == 400 && viewport[3] == 500);
    CHECK(glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_BLEND) && mask[0] && !mask[1] && mask[2] && !mask[3]);
    CHECK(glGetError() == GL_NO_ERROR);
    /* The strip shows whose choice it is, with the panel closed too, and leaves the state as it was. */
    {
        uint8_t top[4] = {0}, below[4] = {0};
        GLint box[4];
        int was_visible = O.visible;
        O.visible = 0;
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
        dd_overlay_turn(DD_OVERLAY_TURN_MINE);
        dd_overlay_draw((void *)1);
        glReadBuffer(GL_BACK);
        glReadPixels(700, O.draw_h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, top);
        glReadPixels(700, O.draw_h - 40, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, below);
        CHECK(top[1] > 150 && top[0] < 120 && below[1] < 60);
        dd_overlay_turn(DD_OVERLAY_TURN_OTHER);
        dd_overlay_draw((void *)1);
        glReadPixels(700, O.draw_h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, top);
        CHECK(top[0] > 180 && top[1] < 180);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        CHECK(!glIsEnabled(GL_SCISSOR_TEST) && mask[0] && !mask[1] && mask[2] && !mask[3]);
        glGetIntegerv(GL_SCISSOR_BOX, box);
        CHECK(glGetError() == GL_NO_ERROR);
        dd_overlay_turn(DD_OVERLAY_TURN_NONE);
        /* The party blocks: the front rank on the right, mine green, the other's orange, stress along the bottom; state put back. */
        {
            const int owner[4] = {DD_OVERLAY_TURN_MINE, DD_OVERLAY_TURN_OTHER, 0, DD_OVERLAY_TURN_MINE};
            const float stress[4] = {200.0f, 0.0f, 50.0f, -3.0f};
            int x, y, w, h;
            uint8_t px[4] = {0};
            glClearColor(0, 0, 0, 1);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClear(GL_COLOR_BUFFER_BIT);
            glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
            dd_overlay_party(4, owner, stress);
            CHECK(O.party_stress[3] == 0.0f);
            dd_overlay_draw((void *)1);
            party_box(0, &x, &y, &w, &h);
            CHECK(x > 3 * w); /* the front rank is the rightmost block */
            glReadPixels(x + w / 2, y + h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            CHECK(px[1] > 150 && px[0] < 120);
            glReadPixels(x + w - 4, y + 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px); /* full stress: white to the end */
            CHECK(px[0] > 200 && px[1] > 200);
            party_box(1, &x, &y, &w, &h);
            glReadPixels(x + w / 2, y + h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            CHECK(px[0] > 180 && px[1] < 180 && px[1] > 90);
            glReadPixels(x + w / 2, y + 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px); /* no stress: dark */
            CHECK(px[0] < 40);
            party_box(2, &x, &y, &w, &h);
            glReadPixels(x + w / 2, y + h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            CHECK(px[0] > 90 && px[0] < 150 && px[1] > 90 && px[1] < 150);
            glReadPixels(x + 4, y + 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px); /* a quarter: white at the start, dark at the end */
            CHECK(px[0] > 200);
            glReadPixels(x + w - 4, y + 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            CHECK(px[0] < 40);
            glReadPixels(x + w / 2, y - 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px); /* nothing below the blocks */
            CHECK(px[0] < 20 && px[1] < 20);
            glGetBooleanv(GL_COLOR_WRITEMASK, mask);
            CHECK(!glIsEnabled(GL_SCISSOR_TEST) && mask[0] && !mask[1] && mask[2] && !mask[3]);
            CHECK(glGetError() == GL_NO_ERROR);
            dd_overlay_party(0, NULL, NULL);
            CHECK(!O.party_count);
            dd_overlay_party(9, owner, stress);
            CHECK(!O.party_count);
        }
        O.visible = was_visible;
        (void)box;
    }
    uint8_t painted[4]={0},outside[4]={0};glReadBuffer(GL_BACK);
    glReadPixels(20,700,1,1,GL_RGBA,GL_UNSIGNED_BYTE,painted);
    glReadPixels(700,400,1,1,GL_RGBA,GL_UNSIGNED_BYTE,outside);
    CHECK(painted[0]>25 && painted[1]>30 && outside[0]<20 && outside[1]<25);
    CHECK(event(BUTTONDOWN, 170, 115, 0) && O.section == 1);
    CHECK(event(BUTTONUP, 170, 115, 0));
    dd_overlay_draw((void *)1);
    if (argc > 1) save_bitmap(argv[1]);
    CHECK(event(BUTTONDOWN, 80, 160, 0));
    CHECK(event(BUTTONUP, 80, 160, 0));
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessageW(&msg);
    CHECK(commands == 1 && command_kind == DD_OVERLAY_CLAIM && command_value == 40);
    CHECK(!event(BUTTONDOWN, 900, 600, 0)); /* Outside the panel still belongs to the game. */
    O.mouse_x = 60;
    O.mouse_y = 180;
    CHECK(event(WHEEL, -3, 0, 0) && O.scroll[1] > 0);
    CHECK(event(KEYDOWN, 0, 0, 27) && !O.visible);
    /* Messages open the panel and wait for OK, in order; the last one closes what it opened. */
    CHECK(dd_overlay_notice("Eerste melding") && O.visible && O.notice_opened && O.notice_count == 1);
    CHECK(dd_overlay_notice("Tweede melding") && O.notice_count == 2);
    dd_overlay_draw((void *)1);
    if (argc > 1) {
        char notice_path[260];
        snprintf(notice_path, sizeof notice_path, "%s.notice.bmp", argv[1]);
        save_bitmap(notice_path);
    }
    {
        int ok_x = 12 + 100, ok_y = 12 + notice_ok_y() + 10;
        CHECK(event(BUTTONDOWN, 12 + 100, 12 + 110, 0) && O.notice_count == 2); /* Next to OK: nothing. */
        event(BUTTONUP, 12 + 100, 12 + 110, 0);
        CHECK(event(BUTTONDOWN, ok_x, ok_y, 0) && O.notice_count == 1 && !strcmp(O.notices[0], "Tweede melding") && O.visible);
        event(BUTTONUP, ok_x, ok_y, 0);
        CHECK(event(BUTTONDOWN, ok_x, ok_y, 0) && !O.notice_count && !O.visible);
        event(BUTTONUP, ok_x, ok_y, 0);
    }
    /* The same message twice is one message, and it opens the panel again. */
    CHECK(dd_overlay_notice("Dubbel") && dd_overlay_notice("Dubbel") && O.notice_count == 1);
    CHECK(event(KEYDOWN, 0, 0, 1073741890) && !O.visible);
    CHECK(dd_overlay_notice("Dubbel") && O.visible && O.notice_count == 1);
    dd_overlay_notice_clear("Dubbel");
    CHECK(!O.notice_count);
    O.visible = 0;
    /* The connection line comes from the runtime, not from the launcher's own connection. */
    dd_overlay_session(DD_OVERLAY_WAITING);
    CHECK(!strcmp(session_text(), "Wacht op de andere speler"));
    dd_overlay_session(DD_OVERLAY_CONNECTED);
    CHECK(!strcmp(session_text(), "Spelers verbonden"));
    dd_overlay_session(DD_OVERLAY_LOST);
    CHECK(strstr(session_text(), "Verbroken") != NULL);
    dd_overlay_session(DD_OVERLAY_REFUSED);
    CHECK(strstr(session_text(), "Host") != NULL);
    /* Without a launcher the panel still tells the connection; it does not claim the campaign is not loaded. */
    {
        int fresh = O.fresh;
        char runtime0 = O.snapshot.runtime[0];
        O.fresh = 0;
        O.snapshot.runtime[0] = 0;
        dd_overlay_session(DD_OVERLAY_CONNECTED);
        CHECK(strstr(status_text(), "launcher") != NULL && !strcmp(session_text(), "Spelers verbonden") && !strcmp(sync_text(), "De gedeelde sessie loopt."));
        dd_overlay_session(DD_OVERLAY_WAITING);
        CHECK(strstr(sync_text(), "Wacht tot") != NULL);
        O.fresh = 1;
        strcpy(O.snapshot.status, "klaar");
        CHECK(!strcmp(status_text(), "klaar"));
        strcpy(O.snapshot.runtime, "regel");
        CHECK(!strcmp(sync_text(), "regel"));
        O.fresh = fresh;
        O.snapshot.runtime[0] = runtime0;
        dd_overlay_session(DD_OVERLAY_REFUSED);
    }
    /* A click outside the panel stays with the game. */
    CHECK(dd_overlay_notice("Melding") && !event(BUTTONDOWN, 900, 500, 0) && O.notice_count == 1);
    /* F9 hides it, F9 shows it again, and the panel the player opened stays open after OK. */
    CHECK(event(KEYDOWN, 0, 0, 1073741890) && !O.visible && O.notice_count == 1);
    CHECK(event(KEYDOWN, 0, 0, 1073741890) && O.visible && !O.notice_opened);
    dd_overlay_notice_clear("Melding");
    CHECK(!O.notice_count && O.visible);
    CHECK(event(KEYDOWN, 0, 0, 1073741890) && !O.visible);
    /* A message that no longer applies closes the panel it opened. */
    CHECK(dd_overlay_notice("Wacht") && O.visible);
    dd_overlay_notice_clear("Iets anders");
    CHECK(O.notice_count == 1 && O.visible);
    dd_overlay_notice_clear("Wacht");
    CHECK(!O.notice_count && !O.visible);
    /* Alt-tab closes the panel; coming back with an unread message opens it again. */
    {
        uint8_t w[56] = {0};
        uint32_t window_event = 0x200;
        memcpy(w, &window_event, 4);
        CHECK(dd_overlay_notice("Terug van alt-tab") && O.visible);
        w[12] = 13;
        CHECK(!dd_overlay_event(w) && !O.visible && O.notice_count == 1);
        w[12] = 12;
        CHECK(!dd_overlay_event(w) && O.visible);
        dd_overlay_notice_clear("Terug van alt-tab");
        CHECK(!O.notice_count);
        O.visible = 0;
        CHECK(!dd_overlay_event(w) && !O.visible); /* Without a message focus changes nothing. */
    }
    /* Overflow drops the oldest, long text is cut safely, and an empty message is refused. */
    {
        static char long_text[2000];
        memset(long_text, 'a', sizeof long_text - 1);
        for (int i = 0; i < NOTICES + 1; i++) CHECK(dd_overlay_notice(long_text));
        CHECK(O.notice_count == NOTICES && strlen(O.notices[0]) == NOTICE_LENGTH - 1);
        dd_overlay_draw((void *)1);
        for (int i = NOTICES; i > 0; i--) notice_remove(0);
        CHECK(!O.visible && !dd_overlay_notice("") && !O.notice_count);
    }
    /* A panel that cannot draw refuses, so the runtime uses the Windows message. */
    O.gl_failed = 1;
    CHECK(!dd_overlay_notice("Geen paneel") && !O.notice_count && !O.visible);
    O.gl_failed = 0;
    CHECK(!event(MOTION, 90, 120, 0));
    /* A torn or stale mapping never authorizes a UI command. */
    s->sequence = 1;
    O.last_read = 0;
    read_snapshot();
    CHECK(!O.fresh);
    int before = commands;
    command(DD_OVERLAY_CLAIM, 41);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessageW(&msg);
    CHECK(commands == before);
    s->sequence = 2;
    s->updated = GetTickCount64() - 6000;
    O.last_read = 0;
    read_snapshot();
    CHECK(!O.fresh);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(context);
    ReleaseDC(window, dc);
    DestroyWindow(window);
    UnmapViewOfFile(s);
    CloseHandle(mapping);
    printf("F9 overlay: actual GL rendering/state restoration, SDL input bounds, sorted hero identity, "
           "scrolling, stale/torn snapshots: %s (%d failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
