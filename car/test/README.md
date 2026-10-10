# 车机端宿主机测试台

在 Linux/WSL 下把**真实车机源码**（`log.c` / `probe.c` / `install.c`，不改动 `src/`）
与 Win32 桩一起编译到宿主机运行功能测试，无需真机。

## 运行

```powershell
wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/test/run_test.sh
```

- 首次需宿主 gcc：`bash .../test/install_gcc.sh`（与交叉工具链无关）
- 通过标准：`结果: PASS=46 FAIL=0` + `TEST_EXIT=0`
- 静态检查（交叉编译器 -Wall -Wextra）：`bash .../test/test_syntax.sh`

## 结构

| 文件 | 职责 |
|------|------|
| `windows.h` | 精简 Win32 类型/常量声明（**DWORD=uint32_t，与 CE ABI 一致**） |
| `winstub.c` | 文件 API → 沙箱目录映射；注册表 → 静态键值表 |
| `test_main.c` | 46 项断言：log 路径选择/回落/环形缓冲/UTF-8、probe 版本与注册表枚举、install 备份覆盖/自路径判断 |
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
