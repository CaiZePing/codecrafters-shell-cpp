#!/bin/bash

set -euo pipefail

echo "正在编译项目..."
cd build
make -j8

echo "编译成功，启动Shell..."
expect -c '
spawn ./shell
send "complete -C hello hi\r"
send "cd ..\r"
send "hi \r"
interact
'

echo "Shell已退出"