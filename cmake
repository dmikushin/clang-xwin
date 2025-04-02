#!/bin/bash
if [ "$MSYSTEM" = "XWIN" ]; then
    # Check if the first argument starts with --build or -E
    if [ "${1#--build}" = "$1" ] && [ "${1#-E}" = "$1" ]; then
        # Prepend -D commands if not starting with --build
        LD_PRELOAD=/opt/xwin/lib/libinsensitive.so /usr/bin/cmake.orig \
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
            -DCMAKE_SYSTEM_NAME=Windows \
            -DCMAKE_C_COMPILER=cc \
            -DCMAKE_CXX_COMPILER=c++ \
            -DCMAKE_RC_COMPILER=llvm-rc "$@"
    else
        # Run cmake without -D commands
        LD_PRELOAD=/opt/xwin/lib/libinsensitive.so /usr/bin/cmake.orig "$@"
    fi
else
    /usr/bin/cmake.orig "$@"
fi
