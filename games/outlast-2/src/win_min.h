#ifndef WIN_MIN_H
#define WIN_MIN_H

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef int BOOL;
typedef long HRESULT;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef unsigned __int64 uintptr_t;
typedef unsigned __int64 size_t;

#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define S_OK ((HRESULT)0L)
#define E_FAIL ((HRESULT)0x80004005L)

#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define GENERIC_READ    (0x80000000L)
#define GENERIC_WRITE   (0x40000000L)
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define CREATE_ALWAYS   2
#define OPEN_ALWAYS     4
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_END        2

#define PAGE_EXECUTE_READWRITE 0x40
#define MEM_COMMIT  0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000

#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;
typedef const GUID *REFIID;

#define WINAPI __stdcall

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
__declspec(dllimport) void* WINAPI GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
__declspec(dllimport) HMODULE WINAPI LoadLibraryA(LPCSTR lpLibFileName);
__declspec(dllimport) DWORD WINAPI GetSystemDirectoryA(LPSTR lpBuffer, DWORD uSize);
__declspec(dllimport) DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
__declspec(dllimport) DWORD WINAPI GetCurrentProcessId(void);
__declspec(dllimport) void WINAPI Sleep(DWORD dwMilliseconds);
__declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE hLibModule);
__declspec(dllimport) HANDLE WINAPI CreateThread(void* lpThreadAttributes, size_t dwStackSize, DWORD (WINAPI *lpStartAddress)(LPVOID), LPVOID lpParameter, DWORD dwCreationFlags, DWORD* lpThreadId);
__declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) DWORD WINAPI SetFilePointer(HANDLE hFile, long lDistanceToMove, long* lpDistanceToMoveHigh, DWORD dwMoveMethod);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE hObject);
__declspec(dllimport) void WINAPI GetLocalTime(LPSYSTEMTIME lpSystemTime);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID lpAddress, size_t dwSize, DWORD flNewProtect, DWORD* lpflOldProtect);
__declspec(dllimport) LPVOID WINAPI VirtualAlloc(LPVOID lpAddress, size_t dwSize, DWORD flAllocationType, DWORD flProtect);
__declspec(dllimport) BOOL WINAPI VirtualFree(LPVOID lpAddress, size_t dwSize, DWORD dwFreeType);
__declspec(dllimport) HANDLE WINAPI GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) void WINAPI ExitProcess(UINT uExitCode);

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE  ((DWORD)-12)

#ifdef __cplusplus
}
#endif

#endif // WIN_MIN_H
