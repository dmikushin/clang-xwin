# Build Windows apps in a Linux container

Build Windows apps in a Linux container using Clang and [XWin](https://github.com/Jake-Shadle/xwin).


## Building

```
docker build -t clang-xwin .
```


## Usage

TODO


## TODO

1. Integrate pacman to install dependencies from MSYS2 UCRT package repository:

* Use MSYS2's pacman recompiled for Linux
* Use MSYS2's `pacman.conf` to configure the package installation procedure

2. Preinstall Wine to run intermediate Windows tools that might be involved in complex build scripts
