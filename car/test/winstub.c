/* winstub.c —— 宿主机 Win32 桩实现（仅测试用）
 * 文件 API：把 \SDMMC\xxx 等 Win32 路径映射到沙箱目录 <SANDBOX>/SDMMC/xxx
 * 注册表 API：静态键值表，可由测试预设，用于验证 probe 逻辑
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

/* ============================================================
 * 窗口/消息层（动态测试）：驱动 main.c 的 WinMain 全链路
 * 测试预压输入消息 → WinMain 消息循环取出 → 窗口过程处理 →
 * InvalidateRect 追加 WM_PAINT → 绘制文本全部记入 UI 轨迹
 * ============================================================ */
#define QMAX 256
static MSG    g_queue[QMAX];
static int    g_qh, g_qt;
static int    g_quit = -1;           /* >=0 时 GetMessage 返回 0 */
static WNDPROC g_wndproc;
static HWND   g_hwnd;
static int    g_dummyBrush;
static char   g_trace[65536];
static size_t g_tlen;

void winstub_push_msg(UINT msg, WPARAM wp, LPARAM lp)
{
    if (g_qt < QMAX) {
        g_queue[g_qt].hwnd = g_hwnd;
        g_queue[g_qt].message = msg;
        g_queue[g_qt].wParam = wp;
        g_queue[g_qt].lParam = lp;
        g_qt++;
    }
}
void winstub_ui_reset(void)
{
    g_qh = g_qt = 0; g_quit = -1; g_tlen = 0; g_trace[0] = 0;
    g_wndproc = NULL; g_hwnd = NULL;
}
const char* winstub_ui_trace(void) { return g_trace; }

static void trace_wide(LPCWSTR w)
{
    while (*w && g_tlen < sizeof(g_trace) - 8) {
        unsigned int c = (unsigned int)*w++;
        if (c < 0x80) g_trace[g_tlen++] = (char)c;
        else if (c < 0x800) {
            g_trace[g_tlen++] = (char)(0xC0 | (c >> 6));
            g_trace[g_tlen++] = (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            g_trace[g_tlen++] = (char)(0xE0 | (c >> 12));
            g_trace[g_tlen++] = (char)(0x80 | ((c >> 6) & 0x3F));
            g_trace[g_tlen++] = (char)(0x80 | (c & 0x3F));
        } else {
            g_trace[g_tlen++] = (char)(0xF0 | (c >> 18));
            g_trace[g_tlen++] = (char)(0x80 | ((c >> 12) & 0x3F));
            g_trace[g_tlen++] = (char)(0x80 | ((c >> 6) & 0x3F));
            g_trace[g_tlen++] = (char)(0x80 | (c & 0x3F));
        }
    }
    if (g_tlen < sizeof(g_trace) - 1) g_trace[g_tlen++] = '\n';
    g_trace[g_tlen] = 0;
}

UINT RegisterClassW(const WNDCLASSW* wc)
{
    g_wndproc = wc->lpfnWndProc;
    return 1;
}
HWND CreateWindowW(LPCWSTR cls, LPCWSTR title, DWORD style,
                   int x, int y, int w, int h,
                   HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    (void)cls; (void)title; (void)style; (void)x; (void)y;
    (void)w; (void)h; (void)parent; (void)menu; (void)inst; (void)param;
    g_hwnd = (HWND)&g_dummyBrush;
    if (g_wndproc) g_wndproc(g_hwnd, WM_CREATE, 0, 0);
    return g_hwnd;
}
BOOL GetMessageW(MSG* m, HWND wnd, UINT minmsg, UINT maxmsg)
{
    (void)wnd; (void)minmsg; (void)maxmsg;
    /* 队列优先（先画完挂起的 WM_PAINT），再判退出 —— 与真机
     * WM_QUIT 特殊语义略异，但保证测试断言可观察到重绘 */
    if (g_qh < g_qt) {
        *m = g_queue[g_qh++];
        m->hwnd = g_hwnd;   /* 压入时窗口尚未创建，出队时回填 */
        return 1;
    }
    return 0;
}
BOOL TranslateMessage(const MSG* m) { (void)m; return 0; }
LRESULT DispatchMessage(const MSG* m)
{
    return g_wndproc ? g_wndproc(m->hwnd, m->message, m->wParam, m->lParam) : 0;
}
void PostQuitMessage(int code) { g_quit = code; }
BOOL DestroyWindow(HWND wnd)
{
    if (g_wndproc) g_wndproc(wnd, WM_DESTROY, 0, 0);
    return TRUE;
}
BOOL InvalidateRect(HWND wnd, const RECT* r, BOOL erase)
{
    (void)r; (void)erase;
    if (g_qt < QMAX) {
        g_queue[g_qt].hwnd = wnd;
        g_queue[g_qt].message = WM_PAINT;
        g_queue[g_qt].wParam = 0;
        g_queue[g_qt].lParam = 0;
        g_qt++;
    }
    return TRUE;
}
BOOL GetClientRect(HWND wnd, RECT* r)
{
    (void)wnd;
    r->left = 0; r->top = 0;
    r->right = GetSystemMetrics(SM_CXSCREEN);
    r->bottom = GetSystemMetrics(SM_CYSCREEN);
    return TRUE;
}
HDC BeginPaint(HWND wnd, PAINTSTRUCT* ps)
{
    (void)wnd;
    memset(ps, 0, sizeof(*ps));
    ps->hdc = (HDC)&g_dummyBrush;
    return ps->hdc;
}
BOOL EndPaint(HWND wnd, const PAINTSTRUCT* ps) { (void)wnd; (void)ps; return TRUE; }
LRESULT DefWindowProcW(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)wnd; (void)msg; (void)wp; (void)lp;
    return 0;
}
HBRUSH CreateSolidBrush(COLORREF c) { (void)c; return (HBRUSH)&g_dummyBrush; }
BOOL DeleteObject(HGDIOBJ obj) { (void)obj; return TRUE; }
int FillRect(HDC dc, const RECT* r, HBRUSH b) { (void)dc; (void)r; (void)b; return 1; }
int DrawTextW(HDC dc, LPCWSTR text, int cch, RECT* r, UINT fmt)
{
    (void)dc; (void)cch; (void)r; (void)fmt;
    if (text) trace_wide(text);
    return 1;
}
int SetBkMode(HDC dc, int mode) { (void)dc; (void)mode; return 1; }
COLORREF SetTextColor(HDC dc, COLORREF c) { (void)dc; return c; }
HCURSOR LoadCursor(HINSTANCE inst, LPCWSTR name)
{
    (void)inst; (void)name;
    return (HCURSOR)&g_dummyBrush;
}
int wsprintfW(wchar_t* buf, LPCWSTR fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vswprintf(buf, 256, fmt, ap);
    va_end(ap);
    return n;
}
