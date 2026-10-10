#ifndef EZ_PROBE_H
#define EZ_PROBE_H

/* 系统基本信息（供界面头部显示） */
typedef struct {
    char os[96];        /* CE 版本 / build / 平台 */
    char cpu[64];       /* 处理器架构 */
    char mem[96];       /* 内存 */
    char res[32];       /* 分辨率 */
    char storage[128];  /* 存储卡状态 */
} ProbeBasic;

void probe_basic(ProbeBasic* out);      /* 快速：版本/架构/内存/分辨率 */
int  probe_deep(void);                  /* 深度：注册表驱动树等全部写入日志，返回写入行数 */

#endif
