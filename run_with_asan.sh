#!/usr/bin/env bash

LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so) ASAN_OPTIONS=detect_leaks=1 $@
