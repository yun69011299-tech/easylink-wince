#ifndef WINSTUB_H
#define WINSTUB_H

/* 测试控制接口（不进车机二进制） */
const char* winstub_sandbox(void);          /* 沙箱根（宿主路径，无尾斜杠） */
void        winstub_set_sandbox(const char* path);
void        winstub_set_self(const char* hostPath);   /* GetModuleFileNameW 返回值 */

void winstub_reg_reset(void);
void winstub_reg_add_key(const char* path);                       /* 声明键存在 */
void winstub_reg_add_sz(const char* path, const char* name, const char* v);
void winstub_reg_add_dword(const char* path, const char* name, unsigned long v);

/* 窗口/消息层（动态测试） */
void        winstub_push_msg(unsigned msg, unsigned long wp, long lp); /* 压入输入消息 */
void        winstub_ui_reset(void);          /* 清空队列/轨迹/退出标志 */
const char* winstub_ui_trace(void);          /* 所有 DrawTextW 输出（UTF-8，\n 分隔） */

#endif
