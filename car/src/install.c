#include <windows.h>
#include <stdio.h>
#include "install.h"
#include "log.h"

/* 凯立德（Careland）标准安装路径 = 这类车机 MAP 键最常见的硬编码启动位置。
 * 单位根目录命名因固件而异（SDMMC / Storage Card / Card），全枚举。 */
static const wchar_t* const kTargets[] = {
    L"\\SDMMC\\NaviOne\\NaviOne.exe",
    L"\\SDMMC\\NaviOne.exe",
    L"\\Storage Card\\NaviOne\\NaviOne.exe",
    L"\\Storage Card\\NaviOne.exe",
    L"\\Card\\NaviOne\\NaviOne.exe",
    L"\\Card\\NaviOne.exe",
    NULL
};

static void ensure_parent_dir(const wchar_t* target)
{
    wchar_t dir[260];
    int i, last = -1;
    for (i = 0; target[i] && i < 259; i++) {
        dir[i] = target[i];
        if (target[i] == L'\\') last = i;
    }
    dir[i] = 0;
    if (last > 0) {
        dir[last] = 0;           /* 去掉文件名 */
        CreateDirectoryW(dir, NULL);   /* 中间目录 NaviOne（已存在则失败无害） */
    }
}

static int same_path(const wchar_t* a, const wchar_t* b)
{
    return lstrcmpiW(a, b) == 0;
}

int install_to_map(void)
{
    wchar_t self[260];
    char p8[300];
    int ok = 0, i;

    if (GetModuleFileNameW(NULL, self, 260) == 0) {
        log_write("安装失败: 无法获取自身路径");
        return 0;
    }
    log_wide2utf8(self, p8, sizeof(p8));
    log_write(">> 安装到 MAP 键启动路径，源: %s", p8);

    for (i = 0; kTargets[i]; i++) {
        const wchar_t* t = kTargets[i];
        DWORD a;
        char t8[300];
        int installed = 0;

        log_wide2utf8(t, t8, sizeof(t8));
        a = GetFileAttributesW(t);

        if (a != 0xFFFFFFFF) {
            log_write("  候选 %s: 已存在", t8);
            if (same_path(self, t)) {
                log_write("  -> 运行中的就是该文件，无需安装");
                ok++;
                continue;
            }
            /* 备份已有文件后覆盖 */
            {
                wchar_t bak[280];
                lstrcpyW(bak, t);
                lstrcatW(bak, L".bak");
                DeleteFileW(bak);
                if (!MoveFileW(t, bak)) {
                    log_write("  备份失败 err=%lu，尝试直接覆盖", (unsigned long)GetLastError());
                    if (!DeleteFileW(t)) {
                        log_write("  覆盖失败 err=%lu，跳过", (unsigned long)GetLastError());
                        continue;
                    }
                } else {
                    log_write("  已备份为 %s.bak", t8);
                }
            }
        } else {
            log_write("  候选 %s: 缺失", t8);
        }

        ensure_parent_dir(t);
        if (CopyFileW(self, t, FALSE)) {
            installed = 1;
        } else {
            log_write("  复制失败 err=%lu", (unsigned long)GetLastError());
        }

        if (installed && GetFileAttributesW(t) != 0xFFFFFFFF) {
            log_write("  -> 安装成功: %s", t8);
            ok++;
        } else if (!installed) {
            log_write("  -> 安装失败: %s", t8);
        }
    }

    if (ok > 0)
        log_write(">> 安装完成: %d 个位置可用，按 MAP 键验证", ok);
    else
        log_write(">> 安装未成功，请点[深度探测]回传日志分析 MAP 键实际路径");
    return ok;
}
