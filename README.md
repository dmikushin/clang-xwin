# Build Windows apps in a Linux container

Build Windows apps in a Linux container using Clang and [XWin](https://github.com/Jake-Shadle/xwin).

Yes, it works very well, moreover ATL and MFC are supported!

We use a preloaded library that emulates basic case-insensitive behavior in Linux, which is essential in Windows build environment.

We integrate MSYS2 to install Windows dependencies with pacman.


## Building

```
docker build -t clang-xwin .
```


## Usage

In the project folder:

```
docker run --rm -it -v /etc/localtime:/etc/localtime:ro -v /etc/timezone:/etc/timezone:ro -v (pwd):(pwd) -w (pwd) -p 22227:22221 marcusmae/clang-xwin:mfc
```

Then log in into the container via SSH:
 
```
ssh localhost -p 22227
```
 
The username specification is ignored, the login succeeds without a password.

Navigate to Makefile folder and use `make` or `make -j12` to build the project for Windows.

The SSH container connection method is designed mainly for compatibility with IDEs. In order to use Visual Studio Code or Zed, connect your preferred IDE to the SSH server exposed by the container:

```bash
zed ssh://localhost:22227/$(pwd)
```

Example Windows package lookup and installation using MSYS2-configured pacman:

```
pacman -Ss boost
pacman -S mingw64/mingw-w64-x86_64-boost
```

## TODO

1. Move XWin-specific CMake/Clang settings into the CMake predefined init, e.g. `~/.config/cmake/init.cmake`, plus CMAKE_PREFIX_PATH for MSYS2

2. Fix crash in `llvm-rc` due to libinsensitive.so

3. Figure out why Ninja is not able to perform partial rebuild with `libinsensitive.so` preloaded, while GNU Make works

4. Preinstall Wine to run intermediate Windows tools that might be involved in complex build scripts
