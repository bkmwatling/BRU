#!/usr/bin/env sh

# use `strings` command to get symbols from executable to identify compiler used
if strings "$1" | grep 'clang' 1>/dev/null 2>&1; then
	LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so) ASAN_OPTIONS=detect_leaks=1 $@
else
	LD_PRELOAD=$(gcc -print-file-name=libasan.so) ASAN_OPTIONS=detect_leaks=1 $@
fi
