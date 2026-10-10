/* winstub.c —— 宿主机 Win32 桩实现（仅测试用）
 * 文件 API：把 \SDMMC\xxx 等 Win32 路径映射到沙箱目录 <SANDBOX>/SDMMC/xxx
 * 注册表 API：静态键值表，可由测试预设，用于验证 probe 逻辑
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "winstub.h"

static char g_self[300] = "SANDBOX/SDMMC/EasyLink.exe";
static char g_sandbox[300] = "SANDBOX";
static DWORD g_lastError = 0;

const char* winstub_sandbox(void) { return g_sandbox; }
void winstub_set_sandbox(const char* path) { snprintf(g_sandbox, sizeof(g_sandbox), "%s", path); }

/* ---------- 路径转换：宽字符 Win 路径 → 沙箱宿主路径 ---------- */
static void wide2mb(const wchar_t* w, char* out, int outmax)
{
    int i = 0;
    while (*w && i < outmax - 1) {
        unsigned int c = (unsigned int)*w++;
        out[i++] = (c < 128) ? (char)c : '?';
    }
    out[i] = 0;
}
static void host_path(const wchar_t* w, char* out, int outmax)
{
    char tmp[300];
    int i;
    wide2mb(w, tmp, sizeof(tmp));
    snprintf(out, outmax, "%s%s", winstub_sandbox(), tmp);  /* "\SDMMC\a" -> "<sb>/SDMMC/a" */
    for (i = 0; out[i]; i++) if (out[i] == '\\') out[i] = '/';
}
void winstub_set_self(const char* hostPath) { snprintf(g_self, sizeof(g_self), "%s", hostPath); }

/* ---------- 文件 ---------- */
HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD share, void* sec,
                   DWORD creation, DWORD attrs, HANDLE tmpl)
{
    char hp[400];
    FILE* f;
    (void)share; (void)sec; (void)attrs; (void)tmpl;
    host_path(name, hp, sizeof(hp));
    if (creation == OPEN_ALWAYS)
        f = fopen(hp, access & GENERIC_WRITE ? "ab" : "rb");
    else
        f = fopen(hp, (access & GENERIC_WRITE) ? "wb" : "rb");
    if (!f) { g_lastError = (DWORD)errno; return INVALID_HANDLE_VALUE; }
    return (HANDLE)f;
}
DWORD SetFilePointer(HANDLE h, LONG dist, LONG* hi, DWORD method)
{
    (void)hi;
    if (method == FILE_END) fseek((FILE*)h, 0, SEEK_END);
    else fseek((FILE*)h, dist, SEEK_SET);
    return (DWORD)ftell((FILE*)h);
}
BOOL WriteFile(HANDLE h, const void* buf, DWORD len, DWORD* written, void* ov)
{
    size_t n = fwrite(buf, 1, len, (FILE*)h);
    fflush((FILE*)h);   /* 测试需即时可读（真机为内核文件无此缓冲问题） */
    (void)ov;
    if (written) *written = (DWORD)n;
    return n == len;
}
BOOL CloseHandle(HANDLE h) { if (h != INVALID_HANDLE_VALUE) fclose((FILE*)h); return TRUE; }
DWORD GetFileAttributesW(LPCWSTR name)
{
    char hp[400]; struct stat st;
    host_path(name, hp, sizeof(hp));
    if (stat(hp, &st) != 0) { g_lastError = (DWORD)errno; return 0xFFFFFFFFul; }
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
BOOL CreateDirectoryW(LPCWSTR name, void* sec)
{
    char hp[400]; (void)sec;
    host_path(name, hp, sizeof(hp));
    if (mkdir(hp, 0755) == 0) return TRUE;
    g_lastError = (DWORD)errno;
    return FALSE;
}
BOOL DeleteFileW(LPCWSTR name)
{
    char hp[400];
    host_path(name, hp, sizeof(hp));
    if (remove(hp) == 0) return TRUE;
    g_lastError = (DWORD)errno;
    return FALSE;
}
BOOL MoveFileW(LPCWSTR from, LPCWSTR to)
{
    char a[400], b[400];
    host_path(from, a, sizeof(a)); host_path(to, b, sizeof(b));
    if (rename(a, b) == 0) return TRUE;
    g_lastError = (DWORD)errno; return FALSE;
}
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists)
{
    char a[400], b[400]; FILE *fi, *fo; char buf[4096]; size_t n;
    host_path(from, a, sizeof(a)); host_path(to, b, sizeof(b));
    if (failIfExists && GetFileAttributesW(to) != 0xFFFFFFFFul) { g_lastError = 183; return FALSE; }
    fi = fopen(a, "rb"); if (!fi) { g_lastError = (DWORD)errno; return FALSE; }
    fo = fopen(b, "wb");
    if (!fo) { fclose(fi); g_lastError = (DWORD)errno; return FALSE; }
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) fwrite(buf, 1, n, fo);
    fclose(fi); fclose(fo);
    return TRUE;
}
DWORD GetModuleFileNameW(HINSTANCE m, LPWSTR buf, DWORD size)
{
    char tmp[400]; const char* host; int i, n = 0;
    (void)m;
    /* 宿主路径 → Win 路径（去沙箱前缀，/ → \） */
    snprintf(tmp, sizeof(tmp), "%s", g_self);
    host = tmp;
    { const char* sb = winstub_sandbox();
      if (strncmp(host, sb, strlen(sb)) == 0) host += strlen(sb); }
    for (i = 0; host[i] && n < (int)size - 1; i++)
        buf[n++] = (host[i] == '/') ? L'\\' : (wchar_t)(unsigned char)host[i];
    buf[n] = 0;
    return (DWORD)n;
}
DWORD GetLastError(void) { return g_lastError; }

/* ---------- 系统 ---------- */
void GetLocalTime(SYSTEMTIME* st)
{
    time_t t = time(NULL); struct tm* tm = localtime(&t);
    st->wYear = (WORD)(tm->tm_year + 1900); st->wMonth = (WORD)(tm->tm_mon + 1);
    st->wDay = (WORD)tm->tm_mday; st->wHour = (WORD)tm->tm_hour;
    st->wMinute = (WORD)tm->tm_min; st->wSecond = (WORD)tm->tm_sec;
    st->wMilliseconds = 0; st->wDayOfWeek = (WORD)tm->tm_wday;
}
BOOL GetVersionExW(OSVERSIONINFOW* v)
{
    v->dwMajorVersion = 5; v->dwMinorVersion = 1; v->dwBuildNumber = 2100; v->dwPlatformId = 2;
    return TRUE;
}
void GetSystemInfo(SYSTEM_INFO* si)
{
    memset(si, 0, sizeof(*si));
    si->wProcessorArchitecture = 5;   /* ARM */
    si->dwPageSize = 4096;
}
void GlobalMemoryStatus(MEMORYSTATUS* ms)
{
    ms->dwMemoryLoad = 47; ms->dwTotalPhys = 65536; ms->dwAvailPhys = 34000;
    ms->dwAvailVirtual = 90000;
}
int GetSystemMetrics(int idx) { return idx == SM_CYSCREEN ? 480 : 800; }

/* ---------- 注册表（静态表） ---------- */
#define RMAX 1024
typedef struct { const char* path; const char* name; int isDword; unsigned long dv; const char* sv; } RVal;
static RVal g_rvals[RMAX];
static int  g_rvalN;
typedef struct { const char* path; } RKey;
static RKey g_rkeys[RMAX];
static int  g_rkeyN;
static HKEY g_open[RMAX];
static char g_openPath[RMAX][240];
static int  g_openN;

void winstub_reg_reset(void) { g_rvalN = 0; g_rkeyN = 0; g_openN = 0; }
void winstub_reg_add_key(const char* path)
{
    if (g_rkeyN < RMAX) g_rkeys[g_rkeyN++].path = path;
}
void winstub_reg_add_sz(const char* path, const char* name, const char* v)
{
    if (g_rvalN < RMAX) { g_rvals[g_rvalN++] = (RVal){path, name, 0, 0, v}; }
}
void winstub_reg_add_dword(const char* path, const char* name, unsigned long v)
{
    if (g_rvalN < RMAX) { g_rvals[g_rvalN++] = (RVal){path, name, 1, v, NULL}; }
}

static void mb2w(const char* s, wchar_t* w, int wmax)
{
    int i = 0; while (*s && i < wmax - 1) w[i++] = (unsigned char)*s++; w[i] = 0;
}
static int str_ieq(const char* a, const char* b) { return strcasecmp(a, b) == 0; }

LONG RegOpenKeyExW(HKEY root, LPCWSTR path, DWORD reserved, DWORD access, HKEY* out)
{
    char p[240]; int i, any = 0;
    (void)root; (void)reserved; (void)access;
    wide2mb(path, p, sizeof(p));
    for (i = 0; i < g_rkeyN; i++)
        if (str_ieq(g_rkeys[i].path, p)) { any = 1; break; }
    for (i = 0; i < g_rvalN && !any; i++)
        if (str_ieq(g_rvals[i].path, p)) { any = 1; break; }
    if (!any) return ERROR_FILE_NOT_FOUND;
    if (g_openN >= RMAX) return 6;   /* ERROR_INVALID_HANDLE */
    snprintf(g_openPath[g_openN], sizeof(g_openPath[0]), "%s", p);
    g_open[g_openN] = (HKEY)(long)(g_openN + 1);
    *out = g_open[g_openN];
    g_openN++;
    return ERROR_SUCCESS;
}

LONG RegEnumValueW(HKEY k, DWORD idx, LPWSTR name, DWORD* nlen, DWORD* reserved,
                   DWORD* type, BYTE* data, DWORD* dlen)
{
    long h = (long)k - 1;
    int i, seen = 0;
    const char* path;
    (void)reserved;
    if (h < 0 || h >= g_openN) return 6;
    path = g_openPath[h];
    for (i = 0; i < g_rvalN; i++) {
        if (!str_ieq(g_rvals[i].path, path)) continue;
        if (seen++ != (int)idx) continue;
        if (g_rvals[i].isDword) {
            if (data && *dlen >= 4) { unsigned long v = g_rvals[i].dv; memcpy(data, &v, 4); }
            if (dlen) *dlen = 4;
            if (type) *type = REG_DWORD;
        } else {
            /* 关键：Linux wchar_t=4 字节（CE 为 2 字节），桩必须按宿主
             * wchar_t 宽度写入——probe 直接把 data 当 wchar_t* 读 */
            wchar_t wbuf[128];
            DWORD wn;
            mb2w(g_rvals[i].sv, wbuf, 128);
            wn = (DWORD)((wcslen(wbuf) + 1) * sizeof(wchar_t));
            if (data && *dlen >= wn) memcpy(data, wbuf, wn);
            if (dlen) *dlen = wn;
            if (type) *type = REG_SZ;
        }
        if (name && nlen) { mb2w(g_rvals[i].name, name, (int)*nlen); *nlen = (DWORD)wcslen(name); }
        return ERROR_SUCCESS;
    }
    return ERROR_NO_MORE_ITEMS;
}
LONG RegEnumKeyExW(HKEY k, DWORD idx, LPWSTR name, DWORD* nlen,
                   DWORD* reserved, LPWSTR cls, DWORD* clen, DWORD* ft)
{
    long h = (long)k - 1;
    int i, seen = 0;
    const char* path;
    (void)reserved; (void)cls; (void)clen; (void)ft;
    if (h < 0 || h >= g_openN) return 6;
    path = g_openPath[h];
    for (i = 0; i < g_rkeyN; i++) {
        const char* kp = g_rkeys[i].path;
        size_t pl = strlen(path);
        const char* rest;
        if (strncasecmp(kp, path, pl) != 0) continue;
        if (kp[pl] != '\\') continue;
        rest = kp + pl + 1;
        if (strchr(rest, '\\')) continue;   /* 只枚举直接子键 */
        if (seen++ != (int)idx) continue;
        if (name && nlen) { mb2w(rest, name, (int)*nlen); *nlen = (DWORD)wcslen(name); }
        return ERROR_SUCCESS;
    }
    return ERROR_NO_MORE_ITEMS;
}
LONG RegCloseKey(HKEY k) { (void)k; return ERROR_SUCCESS; }

/* ---------- 字符串 ---------- */
LPWSTR lstrcpyW(LPWSTR d, LPCWSTR s) { return wcscpy(d, s); }
LPWSTR lstrcatW(LPWSTR d, LPCWSTR s) { return wcscat(d, s); }
int lstrlenW(LPCWSTR s) { return (int)wcslen(s); }
int lstrcmpiW(LPCWSTR a, LPCWSTR b) { return wcscasecmp(a, b); }
