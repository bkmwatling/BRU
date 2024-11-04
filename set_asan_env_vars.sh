export LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so)
export ASAN_OPTIONS=detect_leaks=0
