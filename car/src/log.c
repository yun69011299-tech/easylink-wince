#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "log.h"

#define RING_MAX   128
#define LINE_MAX   192

static char g_ring[RING_MAX][LINE_MAX];
static int  g_ringStart;      /* 最旧行下标 */
static int  g_ringCount;      /* 环内行数 */

static char    g_path[260];
static HANDLE  g_file = INVALID_HANDLE_VALUE;

static const wchar_t* const kCandidates[] = {
    L"\\SDMMC\\easylink_probe.log",
    L"\\Storage Card\\easylink_probe.log",
    L"\\Card\\easylink_probe.log",
    L"\\NAND\\easylink_probe.log",
    L"\\easylink_probe.log",
    NULL
};

int log_init(void)
{
    int i;
    g_path[0] = 0;
    for (i = 0; kCandidates[i]; i++) {
        HANDLE h = CreateFileW(kCandidates[i], GENERIC_WRITE, FILE_SHARE_READ,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            LONG hi = 0;
            g_file = h;
            /* 追加到文件尾 */
            SetFilePointer(h, 0, &hi, FILE_END);
            log_wide2utf8(kCandidates[i], g_path, sizeof(g_path));
            return 1;
        }
    }
    return 0;
}

const char* log_path(void)
{
    return g_path;
}

char* log_wide2utf8(const wchar_t* w, char* dst, int dstSize)
{
    int o = 0;
    if (!dst || dstSize <= 0) return dst;
    while (*w && o < dstSize - 4) {
        unsigned int c = (unsigned int)*w++;
        if (c < 0x80) {
            dst[o++] = (char)c;
        } else if (c < 0x800) {
            dst[o++] = (char)(0xC0 | (c >> 6));
            dst[o++] = (char)(0x80 | (c & 0x3F));
        } else {
            dst[o++] = (char)(0xE0 | (c >> 12));
            dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    dst[o] = 0;
    return dst;
}

void log_write(const char* fmt, ...)
{
    char line[LINE_MAX];
    char body[LINE_MAX - 16];
    SYSTEMTIME st;
    va_list ap;
    int n;

    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    GetLocalTime(&st);
    n = snprintf(line, sizeof(line), "[%02d:%02d:%02d] %s\r\n",
                 st.wHour, st.wMinute, st.wSecond, body);
    if (n < 0 || n >= (int)sizeof(line)) {
        n = sizeof(line) - 3;
        line[n] = '\r'; line[n + 1] = '\n'; line[n + 2] = 0;
    }

    /* 环形缓冲 */
    if (g_ringCount < RING_MAX) {
        strncpy(g_ring[(g_ringStart + g_ringCount) % RING_MAX], line, LINE_MAX - 1);
        g_ring[(g_ringStart + g_ringCount) % RING_MAX][LINE_MAX - 1] = 0;
        g_ringCount++;
    } else {
        strncpy(g_ring[g_ringStart], line, LINE_MAX - 1);
        g_ring[g_ringStart][LINE_MAX - 1] = 0;
        g_ringStart = (g_ringStart + 1) % RING_MAX;
    }

    if (g_file != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(g_file, line, (DWORD)strlen(line), &wr, NULL);
    }
}

void log_clear(void)
{
    g_ringStart = 0;
    g_ringCount = 0;
}

int log_ring_count(void)
{
    return g_ringCount;
}

const char* log_ring_get(int i)
{
    if (i < 0 || i >= g_ringCount) return "";
    return g_ring[(g_ringStart + i) % RING_MAX];
}

void log_close(void)
{
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
}
