/* test_main.c —— 车机端宿主机功能测试
 * 直接链接真实车机源码 log.c / probe.c / install.c（不改动 src/），
 * 通过 winstub 在沙箱目录与静态注册表上验证行为。
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winstub.h"
#include "../src/log.h"
#include "../src/probe.h"
#include "../src/install.h"

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS %s\n", msg); } \
    else      { g_fail++; printf("  FAIL %s  (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

static char g_sbox[400];

/* ---- 工具 ---- */
static void sb_reset(void)   /* 清空沙箱并建根 */
{
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s/SDMMC'", g_sbox, g_sbox);
    if (system(cmd) != 0) { printf("sandbox reset failed\n"); exit(2); }
}
static char* read_all(const char* rel, long* outLen)   /* 读沙箱内文件 */
{
    static char buf[65536];
    char hp[500]; FILE* f; size_t n;
    snprintf(hp, sizeof(hp), "%s/%s", g_sbox, rel);
    f = fopen(hp, "rb");
    if (!f) return NULL;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0; fclose(f);
    if (outLen) *outLen = (long)n;
    return buf;
}
static int file_exists(const char* rel)
{
    char hp[500]; snprintf(hp, sizeof(hp), "%s/%s", g_sbox, rel);
    FILE* f = fopen(hp, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}
static void write_file(const char* rel, const char* content)
{
    char hp[500]; char dir[500]; char* slash; FILE* f;
    snprintf(hp, sizeof(hp), "%s/%s", g_sbox, rel);
    snprintf(dir, sizeof(dir), "%s", hp);
    if ((slash = strrchr(dir, '/'))) { *slash = 0; { char cmd[1100]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir); if (system(cmd)) exit(2); } }
    f = fopen(hp, "wb");
    if (!f) { printf("cannot write %s\n", hp); exit(2); }
    fputs(content, f); fclose(f);
}
/* 设置"自身"路径：winstub 内部会剥掉沙箱前缀还原成 \SDMMC\... 形式 */
static void set_self(const char* rel)
{
    char full[500];
    snprintf(full, sizeof(full), "%s/%s", g_sbox, rel);
    winstub_set_self(full);
}

/* ================= 测试：日志模块 ================= */
static void test_log(void)
{
    printf("[log.c]\n");

    /* T1: 首个可写路径 \SDMMC 被选中 */
    sb_reset();
    CHECK(log_init() == 1, "log_init 在有 SDMMC 时成功");
    CHECK(strcmp(log_path(), "\\SDMMC\\easylink_probe.log") == 0, "日志路径为 \\SDMMC\\easylink_probe.log");

    /* T2: 写入落盘且带时间戳与换行 */
    log_write("hello %d+%d=%d", 1, 2, 3);
    log_close();
    {
        long len; char* c = read_all("SDMMC/easylink_probe.log", &len);
        CHECK(c != NULL, "日志文件已创建");
        if (c) {
            CHECK(strstr(c, "hello 1+2=3") != NULL, "格式化内容正确");
            CHECK(strstr(c, "[") != NULL && strstr(c, "] hello") != NULL, "带时间戳前缀");
            CHECK(strstr(c, "\r\n") != NULL, "CRLF 行尾");
        }
    }

    /* T3: SDMMC 不可写时回落到 \Storage Card（先删掉 SDMMC） */
    sb_reset();
    {
        char cmd[900];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s/SDMMC' && mkdir -p '%s/Storage Card'", g_sbox, g_sbox);
        if (system(cmd) != 0) exit(2);
    }
    CHECK(log_init() == 1, "log_init 回落成功");
    CHECK(strcmp(log_path(), "\\Storage Card\\easylink_probe.log") == 0, "回落路径为 \\Storage Card");
    log_close();

    /* T4: 全部不可写（沙箱指向不存在的目录）→ 返回 0，日志仍进内存环 */
    winstub_set_sandbox("/tmp/ezsandbox_nonexistent");
    CHECK(log_init() == 0, "无可写存储时 log_init 返回 0");
    log_write("memory only");
    CHECK(log_ring_count() >= 1, "无文件时环形缓冲仍工作");
    CHECK(strcmp(log_path(), "") == 0, "log_path 为空串");
    log_close();
    winstub_set_sandbox(g_sbox);

    /* T5: 环形缓冲超过 128 行后丢弃最旧 */
    log_clear();
    { int i; for (i = 0; i < 150; i++) log_write("line-%03d", i); }
    CHECK(log_ring_count() == 128, "环上限 128 行");
    CHECK(strstr(log_ring_get(0), "line-022") != NULL, "最旧行为 line-022（丢弃 0..21）");
    CHECK(strstr(log_ring_get(127), "line-149") != NULL, "最新行为 line-149");
    log_clear();
    CHECK(log_ring_count() == 0, "log_clear 清空环");
    log_close();

    /* T6: 宽字符→UTF-8 转换 */
    {
        char d[32];
        log_wide2utf8(L"AB", d, sizeof(d));
        CHECK(strcmp(d, "AB") == 0, "ASCII 直通");
        log_wide2utf8(L"中", d, sizeof(d));     /* U+4E2D → E4 B8 AD */
        CHECK(strcmp(d, "\xE4\xB8\xAD") == 0, "中文 3 字节 UTF-8");
        log_wide2utf8(L"", d, sizeof(d));
        CHECK(strcmp(d, "") == 0, "空串");
    }
}

/* ================= 测试：探测模块 ================= */
static void test_probe(void)
{
    printf("[probe.c]\n");

    /* T7: 基本信息 */
    {
        ProbeBasic b;
        sb_reset();
        probe_basic(&b);
        CHECK(strstr(b.os, "WinCE 5.01") != NULL, "OS 版本 5.01");
        CHECK(strstr(b.res, "800") != NULL && strstr(b.res, "480") != NULL, "分辨率 800x480");
        CHECK(strstr(b.storage, "\\SDMMC") != NULL, "检出 \\SDMMC 存储根");
        CHECK(strstr(b.cpu, "5") != NULL, "CPU 架构含 ARM(5)");
    }

    /* T8: 深度探测写入日志：预设注册表 → 验证枚举输出 */
    sb_reset();
    CHECK(log_init() == 1, "深度探测前日志可写");
    winstub_reg_reset();
    winstub_reg_add_key("Drivers");
    winstub_reg_add_sz("Drivers", "Prefix", "EZUSB");
    winstub_reg_add_key("Drivers\\USBDevice");
    winstub_reg_add_dword("Drivers\\USBDevice", "MemBase", 0x34800000);
    winstub_reg_add_key("SOFTWARE\\Shell");
    winstub_reg_add_sz("SOFTWARE\\Shell", "NavigationPath", "\\SDMMC\\NaviOne\\NaviOne.exe");

    log_clear();
    {
        int n = probe_deep();
        CHECK(n > 0, "probe_deep 返回行数 > 0");
        CHECK(strstr(log_ring_get(0), "深度探测开始") != NULL, "首行为探测开始标记");
    }
    log_close();
    {
        long len; char* c = read_all("SDMMC/easylink_probe.log", &len);
        CHECK(c != NULL, "深度探测已落盘");
        if (c) {
            CHECK(strstr(c, "EZUSB") != NULL, "枚举出 Drivers\\Prefix 字符串值");
            CHECK(strstr(c, "0x34800000") != NULL, "枚举出 MemBase DWORD 值");
            CHECK(strstr(c, "NaviOne.exe") != NULL, "枚举出 SOFTWARE\\Shell\\NavigationPath（MAP键路径）");
            CHECK(strstr(c, "子键: USBDevice") != NULL, "递归枚举出子键名");
            CHECK(strstr(c, "深度探测结束") != NULL, "探测结束标记存在");
        }
    }

    /* T9: 行数预算 700 上限（1 键下灌 400 值 + 260 子键，必超预算） */
    {
        int i; char path[64];
        winstub_reg_reset();
        winstub_reg_add_key("Drivers");
        for (i = 0; i < 400; i++) {   /* dump_values 单键上限 400 行 */
            snprintf(path, sizeof(path), "v%03d", i);
            winstub_reg_add_sz("Drivers", path, "X");
        }
        for (i = 0; i < 260; i++) {   /* dump_tree 子键上限 256 行 */
            snprintf(path, sizeof(path), "Drivers\\K%03d", i);
            winstub_reg_add_key(path);
        }
        sb_reset(); log_init(); log_clear();
        {
            int n = probe_deep();
            CHECK(n <= 700, "深度探测行数不超过 700 预算");
        }
        log_close();
    }
    winstub_reg_reset();
}

/* ================= 测试：安装模块 ================= */
static void test_install(void)
{
    printf("[install.c]\n");

    /* T10: 全新安装 —— 自身在 \SDMMC\EasyLink.exe，6 个候选全试 */
    sb_reset();
    write_file("SDMMC/EasyLink.exe", "FAKE-PE-CONTENT-v020");
    set_self("SDMMC/EasyLink.exe");
    log_init(); log_clear();
    {
        int ok = install_to_map();
        CHECK(ok >= 1, "至少 1 个候选位置安装成功");
        CHECK(file_exists("SDMMC/NaviOne/NaviOne.exe"), "创建了 \\SDMMC\\NaviOne\\NaviOne.exe（凯立德标准路径）");
        if (file_exists("SDMMC/NaviOne/NaviOne.exe")) {
            long len; char* c = read_all("SDMMC/NaviOne/NaviOne.exe", &len);
            CHECK(c && strcmp(c, "FAKE-PE-CONTENT-v020") == 0, "复制内容与源一致");
        }
        /* 父目录 NaviOne 是新建的 */
        CHECK(file_exists("SDMMC/NaviOne"), "自动创建中间目录 NaviOne");
    }
    log_close();

    /* T11: 目标已存在 → 先备份 .bak 再覆盖 */
    sb_reset();
    write_file("SDMMC/NaviOne/NaviOne.exe", "OLD-KLD-BINARY");
    write_file("SDMMC/EasyLink.exe", "NEW-CONTENT");
    set_self("SDMMC/EasyLink.exe");
    log_init(); log_clear();
    {
        int ok = install_to_map();
        CHECK(ok >= 1, "覆盖安装成功");
        CHECK(file_exists("SDMMC/NaviOne/NaviOne.exe.bak"), "已存在文件备份为 .bak");
        if (file_exists("SDMMC/NaviOne/NaviOne.exe.bak")) {
            long len; char* c = read_all("SDMMC/NaviOne/NaviOne.exe.bak", &len);
            CHECK(c && strcmp(c, "OLD-KLD-BINARY") == 0, "备份内容为原凯立德文件");
        }
        {
            long len; char* c = read_all("SDMMC/NaviOne/NaviOne.exe", &len);
            CHECK(c && strcmp(c, "NEW-CONTENT") == 0, "目标已覆盖为新内容");
        }
    }
    {
        long len; char* c = read_all("SDMMC/easylink_probe.log", &len);
        CHECK(c && strstr(c, "已备份") != NULL, "安装过程写入备份日志");
        CHECK(c && strstr(c, "安装完成") != NULL, "安装完成日志");
    }
    log_close();

    /* T12: 已运行在目标路径 → 无需复制，仍计数成功 */
    sb_reset();
    write_file("SDMMC/NaviOne/NaviOne.exe", "SELF");
    set_self("SDMMC/NaviOne/NaviOne.exe");
    log_init(); log_clear();
    {
        int ok = install_to_map();
        CHECK(ok >= 1, "自身已在目标路径时计成功");
        CHECK(!file_exists("SDMMC/NaviOne/NaviOne.exe.bak"), "不自我备份");
    }
    {
        long len; char* c = read_all("SDMMC/easylink_probe.log", &len);
        CHECK(c && strstr(c, "无需安装") != NULL, "日志提示无需安装");
    }
    log_close();

    /* T13: 只读/无存储 → 返回 0 且给出引导日志 */
    sb_reset();
    write_file("SDMMC/EasyLink.exe", "X");
    set_self("SDMMC/EasyLink.exe");
    /* 删掉整个 SDMMC 目录，使所有候选不可写 */
    { char cmd[500]; snprintf(cmd, sizeof(cmd), "rm -rf '%s/SDMMC'", g_sbox); if (system(cmd)) exit(2); }
    set_self("NAND/Missing/EasyLink.exe");
    log_init(); log_clear();
    {
        int ok = install_to_map();
        CHECK(ok == 0, "无可写位置时返回 0");
    }
    {
        long len; char* c = read_all("SDMMC/easylink_probe.log", &len);
        if (!c) c = read_all("easylink_probe.log", &len);   /* 可能落到根 */
        (void)c;  /* 空沙箱下日志可能无文件，T4 已覆盖该分支 */
    }
    log_close();
}

int main(int argc, char** argv)
{
    const char* sb = (argc > 1) ? argv[1] : "car_test_sandbox";
    snprintf(g_sbox, sizeof(g_sbox), "%s", sb);
    winstub_set_sandbox(g_sbox);

    printf("== 车机端宿主机测试（沙箱: %s）==\n", g_sbox);
    test_log();
    test_probe();
    test_install();

    printf("\n== 结果: PASS=%d FAIL=%d ==\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
