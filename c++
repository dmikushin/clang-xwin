#!/bin/bash
/usr/bin/c++ --target=x86_64-pc-windows-msvc -nostdinc \
    -I/usr/x86_64-w64-mingw32/include \
    -I/opt/xwin/crt/include \
    -I/opt/xwin/sdk/include/ucrt \
    -I/opt/xwin/sdk/include/um \
    -I/opt/xwin/sdk/include/shared \
    -D_WINSOCKAPI_ $@
