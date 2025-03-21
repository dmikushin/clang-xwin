# Build Windows apps in a Linux container

Build Windows apps in a Linux container using Clang and [XWin](https://github.com/Jake-Shadle/xwin).

Yes, it works very well, moreover ATL and MFC are supported!

We use a preloaded library that emulates basic case-insensitive behavior in Linux, which is essential in Windows build environment.


## Building

```
docker build -t clang-xwin .
```


## Usage

In the project folder:

```
docker run --rm -it -v /etc/localtime:/etc/localtime:ro -v /etc/timezone:/etc/timezone:ro -v (pwd):(pwd) -w (pwd) clang-xwin
```

Then navigate to Makefile folder and use `make` or `make -j12` to build the project for Windows.


## TODO

0. Add dropbear without authentication to support remote development with VS Code or Zed

1. Fix crash in `llvm-rc` due to libinsensitive.so

2. Figure out why Ninja is not able to perform partial rebuild with `libinsensitive.so` preloaded, while GNU make works

3. Integrate pacman to install dependencies from MSYS2 UCRT package repository:

* Use MSYS2's pacman recompiled for Linux
* Use MSYS2's `pacman.conf` to configure the package installation procedure

4. Preinstall Wine to run intermediate Windows tools that might be involved in complex build scripts
