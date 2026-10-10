#!/bin/bash
# EasyLink 车机端构建脚本（WSL 内执行）
# 用法（PowerShell）: wsl -d Ubuntu -u root -- bash /mnt/d/LGramfiles/trae/car/build.sh
# 说明: 源码拷入 ext4 编译（避开 DrvFs 大 inode 的 EOVERFLOW），产物回拷 dist/
set -e

REPO=/mnt/d/LGramfiles/trae/car
BUILD=/root/carbuild
GCC=/opt/mingw32ce/bin/arm-mingw32ce-gcc

rm -rf "$BUILD"
mkdir -p "$BUILD"
cp -r "$REPO/src" "$BUILD/src"

cd "$BUILD"
$GCC -O2 -Wall -finput-charset=UTF-8 -o EasyLink.exe src/main.c src/log.c src/probe.c
echo "--- 编译通过"

mkdir -p "$REPO/dist"
cp EasyLink.exe "$REPO/dist/EasyLink-v0.1.0.exe"
echo "BUILD_OK -> $REPO/dist/EasyLink-v0.1.0.exe"
