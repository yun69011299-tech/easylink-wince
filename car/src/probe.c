#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "probe.h"
#include "log.h"

/* ================= 基本信息 ================= */

void probe_basic(ProbeBasic* out)
{
    OSVERSIONINFOW v;
    SYSTEM_INFO si;
    MEMORYSTATUS ms;
    static const wchar_t* const roots[] = {
        L"\\SDMMC", L"\\Storage Card", L"\\Card", L"\\NAND", L"\\Flash", NULL
    };
    char buf[64];
    int i, n = 0;

    memset(out, 0, sizeof(*out));

    memset(&v, 0, sizeof(v));
    v.dwOSVersionInfoSize = sizeof(v);
    if (GetVersionExW(&v)) {
        sprintf(out->os, "WinCE %d.%02d  build %d  plat %d",
                (int)v.dwMajorVersion, (int)v.dwMinorVersion,
                (int)v.dwBuildNumber, (int)v.dwPlatformId);
    } else {
        strcpy(out->os, "WinCE (版本获取失败)");
    }

    memset(&si, 0, sizeof(si));
    GetSystemInfo(&si);
    sprintf(out->cpu, "架构号 %d (0=x86 1=MIPS 2=PPC 3=SH 5=ARM 9=x64) 页=%u",
            (int)si.wProcessorArchitecture,
            (unsigned int)si.dwPageSize);

    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    sprintf(out->mem, "内存占用 %lu%%  物理 %luK 可用 %luK  虚拟可用 %luK",
            (unsigned long)ms.dwMemoryLoad,
            (unsigned long)ms.dwTotalPhys,
            (unsigned long)ms.dwAvailPhys,
            (unsigned long)ms.dwAvailVirtual);

    sprintf(out->res, "%d x %d",
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    for (i = 0; roots[i]; i++) {
        DWORD a = GetFileAttributesW(roots[i]);
        if (a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY)) {
            if (n > 0) strcat(out->storage, ",");
            log_wide2utf8(roots[i], buf, sizeof(buf));
            strcat(out->storage, buf);
        }
    }
    if (out->storage[0] == 0)
        strcpy(out->storage, "(未检测到存储根)");
}

/* ================= 深度探测（注册表驱动树等） ================= */

static int g_budget;
static int g_lines;

static void P(const char* fmt, ...)
{
    char body[192];
    va_list ap;
    if (g_budget <= 0) return;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    log_write("%s", body);
    g_budget--;
    g_lines++;
}

static void dump_values(HKEY k, const char* label)
{
    DWORD idx = 0;
    for (;;) {
        wchar_t name[96];
        BYTE    data[256];
        DWORD   nlen = sizeof(name) / sizeof(wchar_t);
        DWORD   dlen = sizeof(data);
        DWORD   type = 0;
        LONG    r;
        char    n8[192], v8[256];

        name[0] = 0;
        r = RegEnumValueW(k, idx++, name, &nlen, NULL, &type, data, &dlen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) {
            P("%s | 枚举值错误 %ld", label, r);
            break;
        }
        log_wide2utf8(name, n8, sizeof(n8));
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            ((wchar_t*)data)[min(dlen / 2, (DWORD)(sizeof(data) / 2 - 1))] = 0;
            log_wide2utf8((const wchar_t*)data, v8, sizeof(v8));
            P("%s | %s = %s", label, n8, v8);
        } else if (type == REG_DWORD && dlen >= 4) {
            DWORD dv;
            memcpy(&dv, data, 4);
            P("%s | %s = 0x%08lX", label, n8, (unsigned long)dv);
        } else {
            P("%s | %s (type=%lu len=%lu)", label, n8,
              (unsigned long)type, (unsigned long)dlen);
        }
        if (idx > 400) { P("%s | ...值过多已截断", label); break; }
    }
}

/* 列出自身值 + 子键名；depth>0 时递归子键 */
static void dump_tree(const wchar_t* path, int depth, const char* label)
{
    HKEY k;
    LONG r;
    DWORD i;

    if (g_budget <= 0) return;
    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &k);
    if (r != ERROR_SUCCESS) {
        P("打开 %s 失败 (err=%ld)", label, r);
        return;
    }
    dump_values(k, label);

    i = 0;
    for (;;) {
        wchar_t name[96];
        wchar_t child[280];
        DWORD nlen = sizeof(name) / sizeof(wchar_t);
        LONG rr;
        char n8[192], subLabel[240];
        int plen;

        name[0] = 0;
        rr = RegEnumKeyExW(k, i, name, &nlen, NULL, NULL, NULL, NULL);
        if (rr == ERROR_NO_MORE_ITEMS) break;
        if (rr != ERROR_SUCCESS) {
            P("%s | 枚举子键错误 %ld", label, rr);
            break;
        }
        log_wide2utf8(name, n8, sizeof(n8));
        P("%s | 子键: %s", label, n8);

        if (depth > 0) {
            plen = lstrlenW(path);
            if (plen + lstrlenW(name) + 2 < 280) {
                lstrcpyW(child, path);
                lstrcatW(child, L"\\");
                lstrcatW(child, name);
                sprintf(subLabel, "%s\\%s", label, n8);
                dump_tree(child, depth - 1, subLabel);
            }
        }
        i++;
        if (i > 256) { P("%s | 子键过多已截断", label); break; }
    }
    RegCloseKey(k);
}

int probe_deep(void)
{
    ProbeBasic b;

    g_budget = 700;
    g_lines = 0;

    P("======== 深度探测开始 ========");
    probe_basic(&b);
    P("OS: %s", b.os);
    P("CPU: %s", b.cpu);
    P("内存: %s", b.mem);
    P("分辨率: %s", b.res);
    P("存储根: %s", b.storage);

    P("-- HKLM\\Drivers (概览) --");
    dump_tree(L"Drivers", 0, "HKLM\\Drivers");

    P("-- HKLM\\Drivers\\Active (已加载驱动实例) --");
    dump_tree(L"Drivers\\Active", 1, "Drivers\\Active");

    P("-- HKLM\\Drivers\\USBDevice --");
    dump_tree(L"Drivers\\USBDevice", 1, "Drivers\\USBDevice");

    P("-- HKLM\\Drivers\\Touch (触摸) --");
    dump_tree(L"Drivers\\Touch", 1, "Drivers\\Touch");

    P("-- HKLM\\Comm (网络适配器) --");
    dump_tree(L"Comm", 1, "HKLM\\Comm");

    P("-- HKLM\\System\\StorageManager --");
    dump_tree(L"System\\StorageManager", 1, "StorageManager");

    P("-- HKLM\\System (概览) --");
    dump_tree(L"System", 0, "HKLM\\System");

    if (g_budget <= 0)
        P("(达到行数上限，探测被截断)");
    P("======== 深度探测结束，共 %d 行 ========", g_lines);
    return g_lines;
}
