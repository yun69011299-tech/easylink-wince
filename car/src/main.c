/*
 * EasyLink 车机端 v0.2.0
 * 全屏壳 + 系统/驱动探测 + 触摸测试 + SD卡日志回传 +
 * [安装MAP启动]：自复制到凯立德标准导航路径，供 MAP 键硬路径启动。
 */
#include <windows.h>
#include "log.h"
#include "probe.h"
#include "install.h"

#define VER_W   L"v0.2.0"
#define VER_A   "v0.2.0"
#define BTN_N   4

#define WM_CLASS    L"EasyLinkMain"
#define CLR_BG        RGB(16, 20, 26)
#define CLR_HEADER    RGB(24, 60, 96)
#define CLR_HEADER2   RGB(14, 38, 62)
#define CLR_TEXT      RGB(220, 228, 236)
#define CLR_DIM       RGB(140, 152, 166)
#define CLR_BTN       RGB(36, 48, 62)
#define CLR_BTN_HI    RGB(52, 76, 100)
#define CLR_BTN_EDGE  RGB(70, 96, 124)
#define CLR_LOGBG     RGB(8, 10, 12)
#define CLR_LOGTX     RGB(120, 226, 140)
#define CLR_TOUCH     RGB(255, 200, 80)

static HWND       g_hwnd;
static ProbeBasic g_basic;
static RECT       g_btn[BTN_N];
static int        g_btnDown = -1;
static int        g_touchX, g_touchY, g_touchN;

static const wchar_t* const kBtnLabel[BTN_N] = {
    L"深度探测", L"安装MAP启动", L"刷新状态", L"清空日志"
};

/* ---------- 绘制辅助 ---------- */

static void fill_rect(HDC dc, const RECT* r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void frame_rect(HDC dc, const RECT* r, COLORREF c)
{
    /* 部分 CE 导入库缺 FrameRect，用四条边手工描边 */
    RECT e;
    HBRUSH b = CreateSolidBrush(c);
    e.left = r->left; e.top = r->top; e.right = r->right; e.bottom = r->top + 1;
    FillRect(dc, &e, b);
    e.top = r->bottom - 1; e.bottom = r->bottom;
    FillRect(dc, &e, b);
    e.left = r->left; e.top = r->top + 1; e.right = r->left + 1; e.bottom = r->bottom - 1;
    FillRect(dc, &e, b);
    e.left = r->right - 1; e.right = r->right;
    FillRect(dc, &e, b);
    DeleteObject(b);
}

static void text_out(HDC dc, int x, int y, const wchar_t* s, COLORREF c)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + 4096; r.bottom = y + 48;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &r,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
}

/* ASCII/UTF-8 文本 → 屏幕（探测与日志输出多为 UTF-8 中文） */
static void text_out_utf8(HDC dc, int x, int y, const char* s, COLORREF c)
{
    wchar_t w[256];
    int i = 0;
    while (*s && i < 255) {
        unsigned int ch = (unsigned char)*s;
        if (ch < 0x80) {
            w[i++] = (wchar_t)ch; s++;
        } else if ((ch & 0xE0) == 0xC0 && (s[1] & 0x80) == 0x80) {
            w[i++] = (wchar_t)(((ch & 0x1F) << 6) | (s[1] & 0x3F));
            s += 2;
        } else if ((ch & 0xF0) == 0xE0 && (s[1] & 0x80) == 0x80 && (s[2] & 0x80) == 0x80) {
            w[i++] = (wchar_t)(((ch & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
            s += 3;
        } else {
            w[i++] = L'?'; s++;
        }
    }
    w[i] = 0;
    text_out(dc, x, y, w, c);
}

/* ASCII → 宽字符缓冲（用于与宽文本拼接） */
static void ascii_to_wide(const char* s, wchar_t* d, int dmax)
{
    int i = 0;
    while (*s && i < dmax - 1) {
        d[i++] = (wchar_t)(unsigned char)*s++;
    }
    d[i] = 0;
}

/* ---------- 布局 ---------- */

static void layout(int w, int h)
{
    int bx = 8, by, bw, bh, gap = 8, i;
    (void)h;
    bh = 34;
    by = 36 + 4 * 20 + 8;
    bw = (w - bx * 2 - gap * (BTN_N - 1)) / BTN_N;
    for (i = 0; i < BTN_N; i++) {
        g_btn[i].left = bx + i * (bw + gap);
        g_btn[i].top = by;
        g_btn[i].right = g_btn[i].left + bw;
        g_btn[i].bottom = by + bh;
    }
}

static int hit_button(int x, int y)
{
    int i;
    for (i = 0; i < BTN_N; i++)
        if (x >= g_btn[i].left && x < g_btn[i].right &&
            y >= g_btn[i].top && y < g_btn[i].bottom)
            return i;
    return -1;
}

/* ---------- 动作 ---------- */

static void do_probe(void)
{
    int n;
    log_write(">> 开始深度探测");
    n = probe_deep();
    log_write(">> 探测完成 %d 行，请回传日志文件", n);
    InvalidateRect(g_hwnd, NULL, TRUE);
}

/* ---------- 绘制 ---------- */

static void paint(HDC dc, RECT* rc)
{
    wchar_t tmp[260], w2[192];
    RECT r;
    int i, lines, lineH = 18;
    int logTop, logBottom, logLines, first;
    int w = rc->right, h = rc->bottom;

    fill_rect(dc, rc, CLR_BG);

    /* 标题栏 */
    r.left = 0; r.top = 0; r.right = w; r.bottom = 34;
    fill_rect(dc, &r, CLR_HEADER);
    text_out(dc, 8, 9, L"EasyLink 车机端 " VER_W L"  侦察版", CLR_TEXT);
    text_out(dc, w - 108, 11, L"双击标题退出", RGB(180, 210, 240));

    /* 信息区 */
    text_out_utf8(dc, 8, 40, g_basic.os, CLR_TEXT);
    text_out_utf8(dc, 8, 60, g_basic.cpu, CLR_DIM);
    text_out_utf8(dc, 8, 80, g_basic.mem, CLR_DIM);
    lstrcpyW(tmp, L"分辨率 ");
    ascii_to_wide(g_basic.res, w2, 192);
    lstrcatW(tmp, w2);
    lstrcatW(tmp, L" | 存储 ");
    ascii_to_wide(g_basic.storage, w2, 192);
    lstrcatW(tmp, w2);
    text_out(dc, 8, 100, tmp, CLR_DIM);

    /* 按钮 */
    for (i = 0; i < BTN_N; i++) {
        RECT rb = g_btn[i];
        RECT tr;
        fill_rect(dc, &rb, (i == g_btnDown) ? CLR_BTN_HI : CLR_BTN);
        frame_rect(dc, &rb, CLR_BTN_EDGE);
        tr = rb;
        DrawTextW(dc, kBtnLabel[i], -1, &tr,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 日志区 */
    logTop = g_btn[0].bottom + 8;
    logBottom = h - 34;
    if (logBottom - logTop > 36) {
        RECT lr;
        lr.left = 6; lr.top = logTop; lr.right = w - 6; lr.bottom = logBottom;
        fill_rect(dc, &lr, CLR_LOGBG);
        logLines = (logBottom - logTop - 6) / lineH;
        lines = log_ring_count();
        first = (lines > logLines) ? (lines - logLines) : 0;
        for (i = first; i < lines; i++)
            text_out_utf8(dc, 10, logTop + 3 + (i - first) * lineH,
                          log_ring_get(i), CLR_LOGTX);
    } else {
        text_out(dc, 8, logTop, L"(屏幕较矮，日志区已隐藏，请查看 SD 卡日志)", CLR_DIM);
    }

    /* 底栏 */
    r.left = 0; r.top = h - 32; r.right = w; r.bottom = h;
    fill_rect(dc, &r, CLR_HEADER2);
    wsprintfW(tmp, L"触摸: (%d,%d) 次数 %d | 日志: ",
              g_touchX, g_touchY, g_touchN);
    ascii_to_wide(log_path(), w2, 192);
    lstrcatW(tmp, w2);
    text_out(dc, 8, h - 25, tmp, CLR_TOUCH);
}

/* ---------- 窗口过程 ---------- */

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_btnDown = -1;
        if (!log_init())
            log_write("警告: 未找到可写存储，日志仅在内存");
        log_write("EasyLink 车机端 " VER_A " 启动");
        log_write("提示: 按[安装MAP启动]后, MAP键可直接启动本程序");
        probe_basic(&g_basic);
        log_write("OS: %s", g_basic.os);
        log_write("存储: %s", g_basic.storage);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        layout(rc.right, rc.bottom);
        paint(dc, &rc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = (int)(short)LOWORD(lp), y = (int)(short)HIWORD(lp);
        RECT rc;
        g_touchX = x; g_touchY = y; g_touchN++;
        GetClientRect(hwnd, &rc);
        layout(rc.right, rc.bottom);
        if (y >= 34) {
            int b = hit_button(x, y);
            if (b >= 0) {
                g_btnDown = b;
                InvalidateRect(hwnd, NULL, FALSE);
            } else {
                log_write("触摸 (%d,%d)", x, y);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = (int)(short)LOWORD(lp), y = (int)(short)HIWORD(lp);
        int b = hit_button(x, y);
        if (b >= 0 && b == g_btnDown) {
            if (b == 0) {
                do_probe();
            } else if (b == 1) {
                install_to_map();
                InvalidateRect(hwnd, NULL, TRUE);
            } else if (b == 2) {
                probe_basic(&g_basic);
                log_write(">> 状态已刷新");
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                log_clear();
                log_write(">> 日志已清空");
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        g_btnDown = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        if ((int)(short)HIWORD(lp) < 34) {
            log_write(">> 双击标题，退出");
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        log_close();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    WNDCLASSW wc;
    MSG msg;
    int w, h;

    (void)prev; (void)cmd; (void)show;

    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WM_CLASS;
    RegisterClassW(&wc);

    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowW(WM_CLASS, L"EasyLink",
                           WS_POPUP | WS_VISIBLE,
                           0, 0, w, h,
                           NULL, NULL, inst, NULL);
    if (!g_hwnd) return 1;

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
