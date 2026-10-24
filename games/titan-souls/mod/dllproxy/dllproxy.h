#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern void DLLP_OnAttach(void);
extern void DLLP_OnDetach(void);

static HMODULE hVersion;

typedef BOOL (WINAPI *GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *GetFileVersionInfoByHandle_t)(DWORD, HANDLE, LPVOID*, PDWORD);
typedef BOOL (WINAPI *GetFileVersionInfoExA_t)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *GetFileVersionInfoExW_t)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD (WINAPI *GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI *GetFileVersionInfoSizeExA_t)(DWORD, LPCSTR, LPDWORD);
typedef DWORD (WINAPI *GetFileVersionInfoSizeExW_t)(DWORD, LPCWSTR, LPDWORD);
typedef DWORD (WINAPI *GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD (WINAPI *VerFindFileA_t)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
typedef DWORD (WINAPI *VerFindFileW_t)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
typedef DWORD (WINAPI *VerInstallFileA_t)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
typedef DWORD (WINAPI *VerInstallFileW_t)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
typedef DWORD (WINAPI *VerLanguageNameA_t)(DWORD, LPSTR, DWORD);
typedef DWORD (WINAPI *VerLanguageNameW_t)(DWORD, LPWSTR, DWORD);
typedef BOOL (WINAPI *VerQueryValueA_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
typedef BOOL (WINAPI *VerQueryValueW_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

static GetFileVersionInfoA_t GetFileVersionInfoA_fp = NULL;
static GetFileVersionInfoByHandle_t GetFileVersionInfoByHandle_fp = NULL;
static GetFileVersionInfoExA_t GetFileVersionInfoExA_fp = NULL;
static GetFileVersionInfoExW_t GetFileVersionInfoExW_fp = NULL;
static GetFileVersionInfoSizeA_t GetFileVersionInfoSizeA_fp = NULL;
static GetFileVersionInfoSizeExA_t GetFileVersionInfoSizeExA_fp = NULL;
static GetFileVersionInfoSizeExW_t GetFileVersionInfoSizeExW_fp = NULL;
static GetFileVersionInfoSizeW_t GetFileVersionInfoSizeW_fp = NULL;
static GetFileVersionInfoW_t GetFileVersionInfoW_fp = NULL;
static VerFindFileA_t VerFindFileA_fp = NULL;
static VerFindFileW_t VerFindFileW_fp = NULL;
static VerInstallFileA_t VerInstallFileA_fp = NULL;
static VerInstallFileW_t VerInstallFileW_fp = NULL;
static VerLanguageNameA_t VerLanguageNameA_fp = NULL;
static VerLanguageNameW_t VerLanguageNameW_fp = NULL;
static VerQueryValueA_t VerQueryValueA_fp = NULL;
static VerQueryValueW_t VerQueryValueW_fp = NULL;

static void DLLP_Setup(void) {
    WCHAR sysdir[MAX_PATH];
    GetSystemDirectoryW(sysdir, MAX_PATH);
    lstrcatW(sysdir, L"\\version.dll");
    hVersion = LoadLibraryW(sysdir);
    if (!hVersion) return;

    GetFileVersionInfoA_fp = (GetFileVersionInfoA_t)GetProcAddress(hVersion, "GetFileVersionInfoA");
    GetFileVersionInfoByHandle_fp = (GetFileVersionInfoByHandle_t)GetProcAddress(hVersion, "GetFileVersionInfoByHandle");
    GetFileVersionInfoExA_fp = (GetFileVersionInfoExA_t)GetProcAddress(hVersion, "GetFileVersionInfoExA");
    GetFileVersionInfoExW_fp = (GetFileVersionInfoExW_t)GetProcAddress(hVersion, "GetFileVersionInfoExW");
    GetFileVersionInfoSizeA_fp = (GetFileVersionInfoSizeA_t)GetProcAddress(hVersion, "GetFileVersionInfoSizeA");
    GetFileVersionInfoSizeExA_fp = (GetFileVersionInfoSizeExA_t)GetProcAddress(hVersion, "GetFileVersionInfoSizeExA");
    GetFileVersionInfoSizeExW_fp = (GetFileVersionInfoSizeExW_t)GetProcAddress(hVersion, "GetFileVersionInfoSizeExW");
    GetFileVersionInfoSizeW_fp = (GetFileVersionInfoSizeW_t)GetProcAddress(hVersion, "GetFileVersionInfoSizeW");
    GetFileVersionInfoW_fp = (GetFileVersionInfoW_t)GetProcAddress(hVersion, "GetFileVersionInfoW");
    VerFindFileA_fp = (VerFindFileA_t)GetProcAddress(hVersion, "VerFindFileA");
    VerFindFileW_fp = (VerFindFileW_t)GetProcAddress(hVersion, "VerFindFileW");
    VerInstallFileA_fp = (VerInstallFileA_t)GetProcAddress(hVersion, "VerInstallFileA");
    VerInstallFileW_fp = (VerInstallFileW_t)GetProcAddress(hVersion, "VerInstallFileW");
    VerLanguageNameA_fp = (VerLanguageNameA_t)GetProcAddress(hVersion, "VerLanguageNameA");
    VerLanguageNameW_fp = (VerLanguageNameW_t)GetProcAddress(hVersion, "VerLanguageNameW");
    VerQueryValueA_fp = (VerQueryValueA_t)GetProcAddress(hVersion, "VerQueryValueA");
    VerQueryValueW_fp = (VerQueryValueW_t)GetProcAddress(hVersion, "VerQueryValueW");
}

static void DLLP_Cleanup(void) {
    FreeLibrary(hVersion);
    hVersion = NULL;
}

BOOL WINAPI GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!GetFileVersionInfoA_fp) return FALSE;
    return GetFileVersionInfoA_fp(lptstrFilename, dwHandle, dwLen, lpData);
}

BOOL WINAPI GetFileVersionInfoByHandle(DWORD dwFlags, HANDLE hFile, LPVOID *lplpData, PDWORD pdwLen) {
    if (!GetFileVersionInfoByHandle_fp) return FALSE;
    return GetFileVersionInfoByHandle_fp(dwFlags, hFile, lplpData, pdwLen);
}

BOOL WINAPI GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!GetFileVersionInfoExA_fp) return FALSE;
    return GetFileVersionInfoExA_fp(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

BOOL WINAPI GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!GetFileVersionInfoExW_fp) return FALSE;
    return GetFileVersionInfoExW_fp(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (!GetFileVersionInfoSizeA_fp) return 0;
    return GetFileVersionInfoSizeA_fp(lptstrFilename, lpdwHandle);
}

DWORD WINAPI GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle) {
    if (!GetFileVersionInfoSizeExA_fp) return 0;
    return GetFileVersionInfoSizeExA_fp(dwFlags, lpwstrFilename, lpdwHandle);
}

DWORD WINAPI GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle) {
    if (!GetFileVersionInfoSizeExW_fp) return 0;
    return GetFileVersionInfoSizeExW_fp(dwFlags, lpwstrFilename, lpdwHandle);
}

DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (!GetFileVersionInfoSizeW_fp) return 0;
    return GetFileVersionInfoSizeW_fp(lptstrFilename, lpdwHandle);
}

BOOL WINAPI GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!GetFileVersionInfoW_fp) return FALSE;
    return GetFileVersionInfoW_fp(lptstrFilename, dwHandle, dwLen, lpData);
}

DWORD WINAPI VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir, LPSTR szCurDir, PUINT puCurDirLen, LPSTR szDestDir, PUINT puDestDirLen) {
    if (!VerFindFileA_fp) return 0xFFFF;
    return VerFindFileA_fp(uFlags, szFileName, szWinDir, szAppDir, szCurDir, puCurDirLen, szDestDir, puDestDirLen);
}

DWORD WINAPI VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir, LPWSTR szCurDir, PUINT puCurDirLen, LPWSTR szDestDir, PUINT puDestDirLen) {
    if (!VerFindFileW_fp) return 0xFFFF;
    return VerFindFileW_fp(uFlags, szFileName, szWinDir, szAppDir, szCurDir, puCurDirLen, szDestDir, puDestDirLen);
}

DWORD WINAPI VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir, LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT puTmpFileLen) {
    if (!VerInstallFileA_fp) return 0xFFFFFFFFL;
    return VerInstallFileA_fp(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, puTmpFileLen);
}

DWORD WINAPI VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir, LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT puTmpFileLen) {
    if (!VerInstallFileW_fp) return 0xFFFFFFFFL;
    return VerInstallFileW_fp(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, puTmpFileLen);
}

DWORD WINAPI VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang) {
    if (!VerLanguageNameA_fp) return 0;
    return VerLanguageNameA_fp(wLang, szLang, cchLang);
}

DWORD WINAPI VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD cchLang) {
    if (!VerLanguageNameW_fp) return 0;
    return VerLanguageNameW_fp(wLang, szLang, cchLang);
}

BOOL WINAPI VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen) {
    if (!VerQueryValueA_fp) return FALSE;
    return VerQueryValueA_fp(pBlock, lpSubBlock, lplpBuffer, puLen);
}

BOOL WINAPI VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen) {
    if (!VerQueryValueW_fp) return FALSE;
    return VerQueryValueW_fp(pBlock, lpSubBlock, lplpBuffer, puLen);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        DLLP_Setup();
        DLLP_OnAttach();
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        DLLP_OnDetach();
        DLLP_Cleanup();
    }

    return TRUE;
}
