#define WIN32_LEAN_AND_MEAN
#include "dd_log.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

static HANDLE g_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_lock;
static int g_ready;

void dd_log_open(const char *path) {
    if (!g_ready) {
        InitializeCriticalSection(&g_lock);
        g_ready = 1;
    }
    g_file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

void dd_log(const char *fmt, ...) {
    if (!g_ready || g_file == INVALID_HANDLE_VALUE) return;
    char buf[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d.%03d ", t.wYear, t.wMonth, t.wDay, t.wHour,
                     t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof buf - n, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 3) n = sizeof buf - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    EnterCriticalSection(&g_lock);
    DWORD written;
    WriteFile(g_file, buf, (DWORD)n, &written, NULL);
    LeaveCriticalSection(&g_lock);
}

void dd_log_close(void) {
    if (g_file != INVALID_HANDLE_VALUE) CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
}
