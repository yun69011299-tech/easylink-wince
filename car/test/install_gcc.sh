#!/bin/bash
apt-get update -qq && apt-get install -y -qq gcc
echo "GCC_INSTALL=$?"
gcc --version | head -1
