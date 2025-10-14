#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

# Auto-detect LLVM_DIR
if [ -z "$LLVM_DIR" ]; then
  if [ -d /usr/lib/llvm-14/lib/cmake/llvm ]; then
    LLVM_DIR=/usr/lib/llvm-14/lib/cmake/llvm
  else
    LLVM_DIR=$(llvm-config --cmakedir 2>/dev/null || echo "/usr/lib/llvm-14/lib/cmake/llvm")
  fi
fi

export PATH="/usr/lib/llvm-14/bin:$PATH"

cmake -S . -B build -DLLVM_DIR="$LLVM_DIR"
cmake --build build -j

python3 src/driver/obfus_cli.py --src tests/hello.c --preset balanced --seed 12345
