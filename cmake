#!/bin/sh
LD_PRELOAD=/opt/clang-win/bin/libinsensitive.so /usr/bin/cmake \
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=cc \
    -DCMAKE_CXX_COMPILER=c++ \
    -DCMAKE_RC_COMPILER=llvm-rc "$@"
