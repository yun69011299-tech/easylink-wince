#!/bin/bash
# 车机端宿主机功能测试：真实源码 + Win32 桩，Linux/gcc 下编译运行
# 用法（PowerShell）: wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/test/run_test.sh
set -e
REPO=/mnt/d/LGramfiles/trae/car
T=/root/eztest
rm -rf "$T"; mkdir -p "$T"
cp "$REPO"/test/*.c "$REPO"/test/*.h "$T"/
cp "$REPO"/src/*.c "$REPO"/src/*.h "$T"/
cd "$T"
# 源码在同目录：test_main.c 的 ../src 引用改为同目录
sed -i 's#\.\./src/##g' test_main.c
gcc -O1 -g -Wall -Wextra -I. -o eztest test_main.c winstub.c log.c probe.c install.c main.c -D_GNU_SOURCE
echo "--- 编译通过"
./eztest /tmp/ezsandbox
echo "TEST_EXIT=$?"
