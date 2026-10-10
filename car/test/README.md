# 车机端宿主机测试台

在 Linux/WSL 下把**真实车机源码**（`log.c` / `probe.c` / `install.c`，不改动 `src/`）
与 Win32 桩一起编译到宿主机运行功能测试，无需真机。

## 运行

```powershell
wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/test/run_test.sh
```

- 首次需宿主 gcc：`bash .../test/install_gcc.sh`（与交叉工具链无关）
- 通过标准：`结果: PASS=58 FAIL=0` + `TEST_EXIT=0`
- 静态检查（交叉编译器 -Wall -Wextra）：`bash .../test/test_syntax.sh`

## 两层测试

1. **模块功能测试**（46 项）：log / probe / install 直接调用验证。
2. **动态全链路测试**（12 项）：真实 `main.c` 的 `WinMain` → 消息循环 →
   窗口过程 → WM_PAINT 全链路。测试预压"手指"消息序列
   （点[深度探测] → 点[安装MAP启动] → 日志区触碰 → 双击标题退出），
   断言按钮副作用真实落盘、UI 绘制轨迹（标题/按钮/触摸坐标/日志区）真实输出。
   消息层桩提供队列 + FIFO 派发 + InvalidateRect 追加 WM_PAINT，
   `DrawTextW` 输出全部记入 UI 轨迹供断言。

## 结构

| 文件 | 职责 |
|------|------|
| `windows.h` | 精简 Win32 类型/常量/窗口消息 API 声明（**DWORD=uint32_t，与 CE ABI 一致**） |
| `winstub.c` | 文件 API → 沙箱目录映射；注册表 → 静态键值表；消息队列与 UI 轨迹 |
| `test_main.c` | 58 项断言：模块 46 + 动态全链路 12 |
| `run_test.sh` | 拷入 ext4、编译（sed 修正 `../src` 引用）、运行 |
| `test_syntax.sh` | 交叉编译器逐文件 `-Wall -Wextra -fsyntax-only` |

## 坑（已踩过）

- **桩的 `wchar_t` 是 4 字节**（CE 为 2 字节）：桩写注册表字符串值必须按宿主
  `wchar_t` 宽度写，否则 `probe.c` 读出乱码（曾导致 2 项断言失败）。
- **DWORD 宽度**：桩早期用宿主 `long`（8 字节）导致 `%08lX` 打印值错位，
  现已改为 `uint32_t`。改桩类型前先想真机 ABI。
- **stdio 缓冲**：桩 `WriteFile` 必须 `fflush`，否则测试在 `log_close` 前
  读文件会读到旧内容。
- **不可写环境**：WSL 以 root 跑时 `chmod` 拦不住写入，用"沙箱指向不存在目录"
  模拟无存储。
