/* Isolated visual fixture using the real launcher controls. Never starts Steam/game or writes saves. */
#define wWinMain unused_real_launcher_main
#include "../src/main.c"
#undef wWinMain
#ifndef PREVIEW_DPI
#define PREVIEW_DPI 96
#endif
static int preview_ticks;
static LRESULT CALLBACK preview_proc(HWND w, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NOTIFY) {
        LRESULT result = wndproc(w, message, wp, lp);
        const NMHDR *header = (const NMHDR *)lp;
        if (header->idFrom == ID_HEROES && header->code == LVN_ITEMCHANGED) update_heroes();
        return result;
    }
    if (message == WM_CTLCOLORSTATIC) return wndproc(w, message, wp, lp);
    if (message == WM_COMMAND) return 0; /* all session actions disabled in this fixture */
    if (message == WM_TIMER) {
        snprintf(G.runtime_line, sizeof G.runtime_line,
                 "Fictieve status-update %d; lange controletekst om scrollpositie tijdens verversen te "
                 "controleren.",
                 ++preview_ticks);
        update_checks();
        return 0;
    }
    if (message == WM_CLOSE) {
        DestroyWindow(w);
        return 0;
    }
    if (message == WM_DESTROY) {
        if (G.party_font) DeleteObject(G.party_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, message, wp, lp);
}
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE previous, PWSTR command, int show) {
    (void)previous;
    SetProcessDPIAware();
    g_dpi = PREVIEW_DPI;
    INITCOMMONCONTROLSEX controls = {sizeof controls, ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = preview_proc;
    cls.hInstance = inst;
    cls.lpszClassName = L"DDCoopUIPreview";
    cls.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    cls.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    RegisterClassW(&cls);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect = {0, 0, D(1058), D(726)};
    AdjustWindowRect(&rect, style, FALSE);
    G.wnd =
        CreateWindowW(cls.lpszClassName, L"DDCoop UI-test - fictieve gegevens", style, CW_USEDEFAULT,
                      CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, inst, NULL);
    G.font = CreateFontW(-D(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                         L"Segoe UI");
    G.big = CreateFontW(-D(18), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                        L"Segoe UI");
    build_window();
    HWND buttons[] = {G.host, G.stop, G.logs, G.join, G.invite, G.join_code, G.copy_code, G.code, G.friends};
    for (unsigned i = 0; i < sizeof buttons / sizeof buttons[0]; i++)
        EnableWindow(buttons[i], FALSE);
    G.core.role = LR_ROLE_HOST;
    G.core.state = LR_ST_HOST_PLAYING;
    G.core.cfg.coop_slot = 3;
    SendMessageW(G.slot, CB_ADDSTRING, 0, (LPARAM)L"Slot 1: Fictieve campagne");
    SendMessageW(G.slot, CB_ADDSTRING, 0, (LPARAM)L"Slot 3: Een campagne met een lange naam  (tutorial nog niet uitgespeeld)");
    SendMessageW(G.slot, CB_SETCURSEL, 1, 0);
    strcpy(G.core.status, "UI-test met fictieve gegevens; geen Steam, game of saves.");
    for (int i = 0; i < CHK_COUNT; i++)
        set_check(i, 1,
                  "Fictieve controle %d: dit is een lange regel om te controleren dat de volledige tekst "
                  "leesbaar blijft, ook bij een kleinere checklist en hogere schermschaal.",
                  i + 1);
    lr_hero heroes[12] = {0};
    for (int i = 0; i < 12; i++) {
        heroes[i].id = (uint32_t)i + 1;
        snprintf(heroes[i].name, sizeof heroes[i].name, "Hero %02d", i + 1);
        strcpy(heroes[i].cls, i % 2 ? "vestal" : "crusader");
        heroes[i].xp = i * 3;
        heroes[i].status = (i == 1 || i == 4 || i == 8 || i == 11) ? 0 : 1;
    }
    lr_owners_set_roster(&G.core.owners, heroes, 12);
    G.core.owners.owner[4] = LR_PLAYER_GUEST;
    update_checks();
    update_heroes();
    if (wcsstr(command, L"--self-check")) {
        int bad = 0;
        DWORD styles = (DWORD)GetWindowLongPtrW(G.checks, GWL_STYLE);
        if (!(styles & WS_VSCROLL) || !(styles & ES_READONLY) ||
            SendMessageW(G.checks, EM_GETLINECOUNT, 0, 0) <= 8)
            bad++;
        SendMessageW(G.checks, EM_LINESCROLL, 0, 3);
        int first = (int)SendMessageW(G.checks, EM_GETFIRSTVISIBLELINE, 0, 0);
        strcpy(G.runtime_line, "Status gewijzigd");
        update_checks();
        if (first <= 0 || SendMessageW(G.checks, EM_GETFIRSTVISIBLELINE, 0, 0) != first) bad++;
        const int ids[] = {2, 5, 9, 12};
        for (int row = 0; row < 4; row++) {
            LVITEMW item = {0};
            item.mask = LVIF_PARAM;
            item.iItem = row;
            SendMessageW(G.heroes, LVM_GETITEMW, 0, (LPARAM)&item);
            if (item.lParam != ids[row]) bad++;
        }
        /* The real ListView notification must claim Hero 5 after view sorting, not roster index 1. */
        ListView_SetCheckState(G.heroes, 1, TRUE);
        if (G.core.owners.owner[4] != LR_PLAYER_HOST || G.core.owners.owner[1] != LR_PLAYER_HOST) bad++;
        /* The actual launcher publisher preserves sorted hero identity through local UI IPC. */
        G.game=GetCurrentProcess();overlay_publish();
        if(!g_overlay_view || g_overlay_view->magic!=DD_OVERLAY_MAGIC || (g_overlay_view->sequence&1) ||
           g_overlay_view->hero_count!=12 || g_overlay_view->heroes[0].id!=2 || !g_overlay_view->heroes[0].party)bad++;
        overlay_command(DD_OVERLAY_RELEASE,5);
        if(G.core.owners.owner[4]==LR_PLAYER_HOST)bad++;
        overlay_close();G.game=NULL;
        printf("Launcher UI fixture dpi=%d: wrapping, vertical scrollbar, preserved scroll, party rows and "
               "checkbox identity: %s\n",
               PREVIEW_DPI, bad ? "FAILED" : "OK");
        DestroyWindow(G.wnd);
        return bad ? 1 : 0;
    }
    SetTimer(G.wnd, ID_TIMER, 1000, NULL);
    ShowWindow(G.wnd, show);
    UpdateWindow(G.wnd);
    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (IsDialogMessageW(G.wnd, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
