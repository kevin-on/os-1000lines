#!/bin/bash
set -xue

# Path to clang and compiler flags
CC=/opt/homebrew/opt/llvm/bin/clang  # Ubuntu users: use CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fno-stack-protector -ffreestanding -nostdlib"

# Build the file given as first argument
$CC $CFLAGS -c -o debug.o $1

llvm-objdump -d debug.o