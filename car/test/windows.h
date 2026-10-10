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

#endif
