#ifndef EZ_LOG_H
#define EZ_LOG_H

/* 环形日志：内存尾部供界面显示，同时落盘到存储卡（用于用户回传排查） */

int  log_init(void);                 /* 选择可写存储路径并打开文件，返回是否有文件 */
const char* log_path(void);          /* 当前日志文件路径（ASCII），无则返回 "" */
void log_write(const char* fmt, ...);/* 追加一条（带时间戳）：环 + 文件 */
void log_clear(void);                /* 清空环（文件保留） */
void log_close(void);                /* 关闭文件 */
int  log_ring_count(void);           /* 环内保留行数 */
const char* log_ring_get(int i);     /* 0=最旧，count-1=最新 */

/* 宽字符 → UTF-8（仅供日志/探测输出用，返回 dst） */
char* log_wide2utf8(const wchar_t* w, char* dst, int dstSize);

#endif
