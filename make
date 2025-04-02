#!/bin/bash
if [ "$MSYSTEM" = "XWIN" ]; then
    LD_PRELOAD=/opt/xwin/lib/libinsensitive.so /usr/bin/make.orig $@
else
    /usr/bin/make.orig $@
fi
