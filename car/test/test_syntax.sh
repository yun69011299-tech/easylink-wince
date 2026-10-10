#!/bin/bash
# 车机端静态测试：逐文件 -Wall -Wextra 检查
rm -rf /root/cartest
mkdir -p /root/cartest
cp -r /mnt/d/LGramfiles/trae/car/src /root/cartest/src
cd /root/cartest
GCC=/opt/mingw32ce/bin/arm-mingw32ce-gcc
FAIL=0
for f in src/*.c; do
  echo "== $f"
  if ! $GCC -O2 -Wall -Wextra -finput-charset=UTF-8 -fsyntax-only "$f"; then
    FAIL=1
  fi
done
echo "SYNTAX_TEST_FAIL=$FAIL"
