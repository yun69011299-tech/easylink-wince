# AGENTS.md — 开发交接指南

给接手本项目的 AI/人类开发者的完整环境与上下文说明。**动手前先通读"已知坑"一节，每一条都实际踩过。**

## 1. 项目状态一览

- 仓库：`main` 分支，已有 tag `v0.1.0`（车机端侦察版）、`v0.2.0`（MAP 键启动安装）
- 已完成：手机端 APK 逆向（协议见 `dex_out.txt`）、WinCE 交叉编译工具链打通、车机端 v0.1.0（壳+探测+触摸测试+日志）、v0.2.0（`[安装MAP启动]` 自复制到凯立德标准导航路径 `NaviOne\NaviOne.exe` 等 6 个候选 + `HKLM\SOFTWARE` 启动配置探测——适配"MAP 键硬路径启动、缺文件弹文件管理器"的凯立德时代启动方式）
- 下一步（按序）：
  1. 收集用户车机运行 `EasyLink-v0.2.0.exe` 的日志（重点：点 `[安装MAP启动]` 后 MAP 键是否直接启动；若失败则 `深度探测` 日志中 `HKLM\SOFTWARE\Shell` 会暴露 MAP 键真实启动路径）→ **决定连接通道方案**（ADB-over-USB vs RNDIS 网络 vs 平台 USB 驱动直连）
  2. 手机端：安装 JDK + Android SDK（工具装 `D:\LGprogram\wince` 下），新写 Android 投屏 App（MediaProjection + MediaCodec，监听 `0.0.0.0` 而非回环）
  3. 协议 EC47 实现（车机端收流、手机端推流）
  4. 触摸回传（车机 WM_LBUTTONDOWN → 协议 → 手机 Accessibility 注入）
- 用户环境约束：**所有工具/SDK 必须安装在 `D:\LGprogram\wince`**；车机 WinCE 5/6、ARM、约 800x480、**仅 USB 无 WiFi**、可从 SD 卡运行 exe；目标体验"尽量还原 CarLife"

## 2. 环境事实（本机已装好的东西）

| 组件 | 位置 |
|------|------|
| WSL 发行版 Ubuntu（WSL2） | 机器名 `Ubuntu`，磁盘文件在 `D:\LGprogram\wince\wsl` |
| ARM-WinCE 交叉工具链源 | `D:\LGprogram\wince\toolchain\opt\mingw32ce`（来自 SF 项目 cegcc 的 `mingw32ce-0.59.1.tar.bz2`，GCC 4.4.0，target=`arm-mingw32ce`） |
| 工具链运行位置 | WSL 内 `/opt/mingw32ce`（**ext4 实体拷贝**，非符号链接——原因见坑 K1） |
| 构建脚本 | `car/build.sh`（在 WSL 内跑） |

PowerShell 调用模板：

```powershell
wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/build.sh
```

### 从零重建车机端环境（换机器时照做）

```powershell
# 1. 装 WSL（需管理员 UAC 提权一次）
wsl --install -d Ubuntu --no-launch --location D:\LGprogram\wince\wsl
# 2. 下载工具链（SourceForge cegcc 项目，直接下载不被 Cloudflare 拦）
curl.exe -L -o D:\LGprogram\wince\mingw32ce-0.59.1.tar.bz2 `
  "https://downloads.sourceforge.net/project/cegcc/cegcc/0.59.1/mingw32ce-0.59.1.tar.bz2"
tar -xjf D:\LGprogram\wince\mingw32ce-0.59.1.tar.bz2 -C D:\LGprogram\wince\toolchain
```

```bash
# 3. WSL 内（root）：32 位运行库 + soname 兼容链接 + 工具链入 ext4
dpkg --add-architecture i386 && apt-get update
apt-get install -y libc6:i386 zlib1g:i386 libgmp10:i386 libmpfr6:i386
ln -sfn /usr/lib/i386-linux-gnu/libgmp.so.10 /usr/lib/i386-linux-gnu/libgmp.so.3
ln -sfn /usr/lib/i386-linux-gnu/libmpfr.so.6 /usr/lib/i386-linux-gnu/libmpfr.so.1
ldconfig
cp -a /mnt/d/LGprogram/wince/toolchain/opt/mingw32ce /opt/mingw32ce
# 4. 冒烟验证：应产出 machine=ARM subsystem=9(Windows CE) 的 PE
```

## 3. 已知坑（全部实测踩过）

- **K1 DrvFs EOVERFLOW**：工具链是 2009 年的 32 位 ELF，在 `/mnt/d`（DrvFs，64 位大 inode）上 `stat` 会报 `Value too large for defined data type`，连 gcc 找 cc1 都会失败。**必须把源码拷进 ext4（/root 等）编译，再把产物 `cp` 回 /mnt/d**（cp 是 64 位程序，回拷没问题）。`build.sh` 已按此实现。
- **K2 `-mwindows` 不可用**：该 GCC 未实现此参数（cc1 报 unrecognized），**不传即可**——默认输出就是 subsystem=9 (Windows CE)，WinMain 链接正常。
- **K3 CE 导入库缺口**：`TextOutW`、`FrameRect` 未导出（nm 还读不了这些库的格式）。用 `DrawTextW` 代替文字输出；边框用 4 条 1px `FillRect` 手工画。**链接报 undefined reference 时先怀疑库缺函数，不是代码错。**
- **K4 WSL interop 引号吞噬**：`wsl ... bash -c "含 $ | " 的命令` 会被 Windows 侧解析破坏。**一律把命令写成 `.sh` 文件放盘上再 `bash /mnt/.../xx.sh`。**
- **K5 控制台乱码**：wsl.exe 输出 UTF-16，PowerShell 重定向变乱码。用 `Start-Process -RedirectStandardOutput` + `[IO.File]::ReadAllText($f, [Text.Encoding]::Unicode)` 读。
- **K6 网络**：raw.githubusercontent.com 与 sourceforge.net 的**文件列表页**可能被墙/Cloudflare 挑战；**直链下载**（downloads.sourceforge.net）通常可用。GitHub 连接重置时让用户修网络。
- **K7 安装需要提权**：本会话默认非管理员，装 WSL 用 `Start-Process -Verb RunAs` 弹 UAC 让用户点。
- **K8 老 GCC 的 CRT**：`snprintf/vsnprintf` 可用；**CE 无 ANSI 版 API，全部用 `W` 后缀函数**（`CreateWindowW` 等）；日志文件内容用 UTF-8，界面宽字符解码见 `main.c` 的 `text_out_utf8`。

## 4. 协议 EC47（逆向结论，`dex_out.txt` 为唯一权威依据）

**已确认**（来自反汇编，勿凭猜测改写）：

- 手机端监听：`ServerSocket(port=40690, backlog=1, bind=127.0.0.1)` —— 回环绑定是原方案痛点，我们自研通道时**新手机端应绑 0.0.0.0 或按通道设计**
- 帧格式（`DataOutputStream`，**大端**）：`int32 magic=0x45433437("EC47") | uint8 type | uint8 flags | int64 presentationTimeUs | int32 len | bytes[len]`
- 握手文本帧：`EC47;h264;baseline;port=40690`
- `type=2`：视频格式帧，payload=`MediaFormat.toString()` 文本
- `type=9`：错误帧（文本，`reportError` 写入）
- 车机→手机输入为**按空格分割的文本行**（换行分隔）：`tap x y`（注入点击）、`drag x1 y1 x2 y2 duration`（注入拖动），坐标为浮点
- 编码：H.264 baseline；无音频通道
- **待实测确认**：hello 帧与视频帧的确切 type 值（反汇编中为寄存器常量，`sendVideo` 附近见 `dex_out.txt` 行 361-371）——实现时先抓包或对照读

## 5. 车机端代码地图（v0.1.0）

- `main.c`：全屏窗口（`WS_POPUP`）、按钮命中、`paint()` 三段布局（标题栏 34px / 信息+按钮 / 日志区 / 底栏触摸状态）；双击标题退出
- `probe.c`：`probe_basic()` 读版本/架构/内存/分辨率/存储根；`probe_deep()` 递归 dump 注册表 `Drivers`、`Drivers\Active`、`Drivers\USBDevice`、`Drivers\Touch`、`Comm`、`StorageManager`（预算 700 行防刷屏）
- `log.c`：128 行环形缓冲（界面尾部显示）+ 落盘（候选路径依次 `\\SDMMC\` → `\\Storage Card\` → `\\Card\` → `\\NAND\` → `\\`，UTF-8 追加）
- 静态验证命令：

```powershell
# PE 头检查（也应看到 imports 仅 coredll.dll）
$b=[IO.File]::ReadAllBytes("car\dist\EasyLink-v0.1.0.exe"); $pe=[BitConverter]::ToInt32($b,0x3c)
[BitConverter]::ToUInt16($b,$pe+4)    # 0x1c0 = ARM
[BitConverter]::ToUInt16($b,$pe+92)   # subsystem: 9 = Windows CE
```

## 5.5 测试（无需真机）

```powershell
# 宿主机功能测试：58 项断言 = 模块 46 + main.c 动态全链路 12（WinMain 消息循环/触摸/重绘）
wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/test/run_test.sh
# 静态检查：交叉编译器逐文件 -Wall -Wextra
wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/test/test_syntax.sh
```

细节与坑见 `car/test/README.md`。改 `src/` 后必须两者全绿再提交。

## 6. 版本与提交约定

- 提交信息：中文 `<模块>: <说明>`（例：`车机端: 修复触摸坐标越界`）
- 发版：附注 tag `vX.Y.Z` + 更新 `car/dist/` 产物 + 更新本文件"项目状态一览"
- 改动协议相关必须回引 `dex_out.txt` 行号作为依据
