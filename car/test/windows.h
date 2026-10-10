/* windows.h —— 宿主机测试用 Win32 桩头（仅供单元测试编译，不进发布产物）
 * 使真实车机源码（log.c/probe.c/install.c）可在 Linux/gcc 下编译运行。
 * 约定：DWORD/LONG 用宿主机 long（64 位）——仅测试用，不依赖其宽度。 */
#ifndef FAKE_WINDOWS_H
#define FAKE_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

/* 必须与 WinCE 一致：DWORD=4 字节（真机 ABI 依赖宽度，勿用宿主 long） */
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef int32_t  LONG;
typedef int          BOOL;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef void*        HANDLE;
typedef void*        HKEY;
typedef void*        HINSTANCE;
typedef wchar_t      WCHAR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t*     LPWSTR;
typedef char*        LPSTR;
typedef const char*  LPCSTR;

#define WINAPI
#define CALLBACK

#define TRUE  1
#define FALSE 0

#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFul

#define GENERIC_WRITE        0x40000000ul
#define FILE_SHARE_READ      0x00000001ul
#define OPEN_ALWAYS          4ul
#define FILE_ATTRIBUTE_NORMAL 0x80ul
#define FILE_ATTRIBUTE_DIRECTORY 0x10ul
#define FILE_END             2ul

#define ERROR_SUCCESS        0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_NO_MORE_ITEMS  259L

#define REG_SZ        1ul
#define REG_EXPAND_SZ 2ul
#define REG_DWORD     4ul
#define KEY_READ      0x20019ul

#define HKEY_LOCAL_MACHINE ((HKEY)(long)0x80000002ul)

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

#define min(a,b) (((a)<(b))?(a):(b))

typedef struct {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;

typedef struct {
    DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
    WCHAR szCSDVersion[128];
} OSVERSIONINFOW;

typedef struct {
    DWORD dwOemId;
    DWORD wProcessorArchitecture;   /* 实际为 WORD，测试桩放宽 */
    DWORD dwPageSize;
    void *lpMinimumApplicationAddress, *lpMaximumApplicationAddress;
    DWORD dwActiveProcessorMask, dwNumberOfProcessors, dwProcessorType;
    DWORD dwAllocationGranularity, wProcessorLevel, wProcessorRevision;
} SYSTEM_INFO;

typedef struct {
    DWORD dwLength, dwMemoryLoad, dwTotalPhys, dwAvailPhys,
          dwTotalPageFile, dwAvailPageFile, dwTotalVirtual, dwAvailVirtual;
} MEMORYSTATUS;

/* ---- 文件 ---- */
HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD share, void* sec,
                   DWORD creation, DWORD attrs, HANDLE tmpl);
DWORD  SetFilePointer(HANDLE h, LONG dist, LONG* hi, DWORD method);
BOOL   WriteFile(HANDLE h, const void* buf, DWORD len, DWORD* written, void* ov);
BOOL   CloseHandle(HANDLE h);
DWORD  GetFileAttributesW(LPCWSTR name);
BOOL   CreateDirectoryW(LPCWSTR name, void* sec);
BOOL   DeleteFileW(LPCWSTR name);
BOOL   MoveFileW(LPCWSTR from, LPCWSTR to);
BOOL   CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists);
DWORD  GetModuleFileNameW(HINSTANCE m, LPWSTR buf, DWORD size);
DWORD  GetLastError(void);

/* ---- 系统 ---- */
void   GetLocalTime(SYSTEMTIME* st);
BOOL   GetVersionExW(OSVERSIONINFOW* v);
void   GetSystemInfo(SYSTEM_INFO* si);
void   GlobalMemoryStatus(MEMORYSTATUS* ms);
int    GetSystemMetrics(int idx);

/* ---- 注册表 ---- */
LONG   RegOpenKeyExW(HKEY root, LPCWSTR path, DWORD reserved, DWORD access, HKEY* out);
LONG   RegEnumKeyExW(HKEY k, DWORD idx, LPWSTR name, DWORD* nlen,
                     DWORD* reserved, LPWSTR cls, DWORD* clen, DWORD* ft);
LONG   RegEnumValueW(HKEY k, DWORD idx, LPWSTR name, DWORD* nlen, DWORD* reserved,
                     DWORD* type, BYTE* data, DWORD* dlen);
LONG   RegCloseKey(HKEY k);

/* ---- 字符串 ---- */
LPWSTR lstrcpyW(LPWSTR d, LPCWSTR s);
LPWSTR lstrcatW(LPWSTR d, LPCWSTR s);
int    lstrlenW(LPCWSTR s);
int    lstrcmpiW(LPCWSTR a, LPCWSTR b);

/* ============================================================
 * 以下为窗口/消息层（动态测试用）：让 main.c 的 WinMain→消息循环→
 * 窗口过程→触摸→WM_PAINT 全链路可在宿主机真实运行
 * ============================================================ */
#include <string.h>

typedef void*    HWND;
typedef void*    HDC;
typedef void*    HBRUSH;
typedef void*    HGDIOBJ;
typedef void*    HCURSOR;
typedef void*    HICON;
typedef void*    HMENU;
typedef intptr_t  LPARAM;
typedef uintptr_t WPARAM;
typedef intptr_t  LRESULT;
typedef DWORD     COLORREF;

typedef struct { int left, top, right, bottom; } RECT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
typedef struct {
    HDC hdc; BOOL fErase; RECT rcPaint;
    BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[128];
} PAINTSTRUCT;
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor;
    LPCWSTR lpszMenuName, lpszClassName;
} WNDCLASSW;

#define WM_CREATE    0x0001
#define WM_DESTROY   0x0002
#define WM_PAINT     0x000F
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_LBUTTONDBLCLK 0x0203

#define WS_POPUP     0x80000000ul
#define WS_VISIBLE   0x10000000ul
#define CS_DBLCLKS   0x0008
#define IDC_ARROW    ((LPCWSTR)(long)32512)

#define RGB(r,g,b) ((COLORREF)(((r)&0xff)|(((g)&0xff)<<8)|(((b)&0xff)<<16)))

#define DT_LEFT       0x0000
#define DT_TOP        0x0000
#define DT_CENTER     0x0001
#define DT_VCENTER    0x0004
#define DT_SINGLELINE 0x0020
#define DT_NOPREFIX   0x0400
#define TRANSPARENT   1

/* main.c 用 Win32 标准名取 lParam 低/高 16 位 */
#define LOWORD(l) ((WORD)(((uintptr_t)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((uintptr_t)(l)) >> 16) & 0xffff))

typedef void* LPVOID;

UINT    RegisterClassW(const WNDCLASSW* wc);
HWND    CreateWindowW(LPCWSTR cls, LPCWSTR title, DWORD style,
                      int x, int y, int w, int h,
                      HWND parent, HMENU menu, HINSTANCE inst, LPVOID param);
BOOL    GetMessageW(MSG* m, HWND wnd, UINT min, UINT max);
BOOL    TranslateMessage(const MSG* m);
LRESULT DispatchMessage(const MSG* m);
#define DispatchMessageW DispatchMessage
void    PostQuitMessage(int code);
BOOL    DestroyWindow(HWND wnd);
BOOL    InvalidateRect(HWND wnd, const RECT* r, BOOL erase);
BOOL    GetClientRect(HWND wnd, RECT* r);
HDC     BeginPaint(HWND wnd, PAINTSTRUCT* ps);
BOOL    EndPaint(HWND wnd, const PAINTSTRUCT* ps);
LRESULT DefWindowProcW(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
HBRUSH  CreateSolidBrush(COLORREF c);
BOOL    DeleteObject(HGDIOBJ obj);
int     FillRect(HDC dc, const RECT* r, HBRUSH b);
int     DrawTextW(HDC dc, LPCWSTR text, int cch, RECT* r, UINT fmt);
int     SetBkMode(HDC dc, int mode);
COLORREF SetTextColor(HDC dc, COLORREF c);
HCURSOR LoadCursor(HINSTANCE inst, LPCWSTR name);
int     wsprintfW(wchar_t* buf, LPCWSTR fmt, ...);

#endif
