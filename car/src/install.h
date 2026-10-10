#ifndef EZ_INSTALL_H
#define EZ_INSTALL_H

/* 把本程序复制到凯立德时代 MAP 键硬编码的导航路径，使 MAP 键可直接启动。
 * 返回成功安装（或已是最新）的数量。全过程写日志。 */
int install_to_map(void);

#endif
