FROM cachyos/cachyos:latest as rootfs

# Copy the v3 optimized pacman configuration
COPY pacman-v3.conf /etc/pacman.conf

# Update system with v3 optimized packages
RUN pacman -Syu strace pkgfile --noconfirm && \
    rm -rf /var/lib/pacman/sync/* && \
    find /var/cache/pacman/ -type f -delete

FROM scratch AS clang

COPY --from=rootfs / /

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8
ENV LC_ALL C.UTF-8

# Install LLVM toolchain
RUN pacman -Sy --noconfirm git curl wget ca-certificates \
    llvm clang lld compiler-rt libc++ libc++abi \
    cmake make ninja meson fish tmux mc openssh gnupg gpgme pcre systemd \
    patch libarchive file strace vim && \
    ln -sf /usr/bin/clang /usr/bin/cc && \
    ln -sf /usr/bin/clang++ /usr/bin/c++ && \ 
    ln -sf /usr/bin/ld.lld /usr/bin/ld && \
    ln -sf /usr/bin/llvm-ar /usr/bin/ar && \
    cc -v && \
    c++ -v && \
    ld -v && \
    ar --version && \
    rm -rf /var/lib/pacman/sync/* && \
    find /var/cache/pacman/ -type f -delete

FROM clang as pacman

# Install build dependencies for pacman
RUN pacman -Sy --noconfirm pkgconf openssl && \
    rm -rf /var/lib/pacman/sync/* && \
    find /var/cache/pacman/ -type f -delete

# We use MSYS2, which is pacman-flavored Windows packages provider that makes libraries
# dependencies installation as easy as in Linux.
# The base container is also managed by pacman. The two different versions of pacman
# are made to co-exist and share the same conf.
RUN set -eux; \
    git clone https://github.com/msys2/msys2-pacman.git && \
    cd msys2-pacman && \
    meson setup builddir --buildtype=release --prefix=/opt/pacman-msys2 --sysconfdir=/etc && \
    meson compile -C builddir && \
    meson install -C builddir

FROM clang

LABEL maintainer="dmitry@kernelgen.org"

ARG INCLUDE_ATL=
ARG INCLUDE_MFC=
#ARG INCLUDE_ATL="--include-atl"
#ARG INCLUDE_MFC="--include-mfc"

# Build & install Dropbear SSH server with our mods
RUN set -eux; \
    git clone https://github.com/dmikushin/dropbear.git && \
    cd dropbear && \
    mkdir build && \
    cd build && \
    CFLAGS="-g -O3" ../configure && \
    make && \
    make install && \
    cd ../.. && \
    rm -rf dropbear && \
    mkdir -p /etc/dropbear

ARG xwin_version="0.6.6-rc.2-superewald"
ARG xwin_suffix="x86_64-unknown-linux-musl"
ARG xwin_base_url="https://github.com/dmikushin/xwin/releases/download"

# Install the Windows headers and libraries provided by XWin project
RUN set -eux; \
    curl --fail -L ${xwin_base_url}/${xwin_version}/xwin-${xwin_version}-${xwin_suffix}.tar.gz | tar -xz && \
    cd xwin-${xwin_version}-${xwin_suffix} && \
    XWIN_ACCEPT_LICENSE=true ./xwin ${INCLUDE_ATL} ${INCLUDE_MFC} --include-debug-runtime splat --include-debug-libs && \
    mv .xwin-cache/splat /opt/xwin && \
    cd .. && \
    rm -rf xwin*

WORKDIR /usr/bin

RUN \
    mv cc cc.orig && \
    mv c++ c++.orig && \
    mv llvm-rc llvm-rc.orig && \
    mv make make.orig && \
    mv cmake cmake.orig && \
    mv ninja ninja.orig

COPY cc .
COPY c++ .
COPY llvm-rc .
COPY make .
COPY cmake .
COPY ninja .

ENV LDFLAGS "-L/opt/xwin/sdk/Lib/um/x86_64 -L/opt/xwin/crt/lib/x86_64 -L/opt/xwin/sdk/lib/ucrt/x86_64 -L/usr/x86_64-w64-mingw32/lib"

WORKDIR /opt/xwin/lib

COPY insensitive.cpp .

RUN clang++ -O3 -std=c++17 -fPIC insensitive.cpp -shared -o libinsensitive.so && \
    rm -rf insensitive.cpp

ENV CLICOLOR_FORCE 1

ENV SHELL /usr/bin/fish

#ENV INSENSITIVE_DEBUG 1

#ENV INSENSITIVE_DEBUG_LEVEL TRACE

ENV DOCKER 1

RUN chsh -s /usr/bin/fish root

RUN echo -e "# /etc/shells: always use fish as a login shell\n/usr/bin/fish\n/bin/bash" >/etc/shells

# Pre-generate Dropbear SSH keys
RUN mkdir -p /etc/dropbear && \
    dropbearkey -t rsa -f /etc/dropbear/dropbear_rsa_host_key && \
    dropbearkey -t ed25519 -f /etc/dropbear/dropbear_ed25519_host_key && \
    dropbearkey -t ecdsa -f /etc/dropbear/dropbear_ecdsa_host_key

# Install MSYS2 pacman and configure it for fetching MSYS2 packages.
# This way we can bring the entire opensource package universe together
# with our Clang-based Windows toolchain.
COPY --from=pacman /opt/pacman-msys2 /opt/pacman-msys2

COPY makepkg-xwin /usr/bin/makepkg-xwin
 
RUN chmod +x /usr/bin/makepkg-xwin
 
COPY makepkg_xwin.conf /etc/makepkg_xwin.conf
 
# Add pacman repo for XWin

RUN ln -s /opt/pacman-msys2/bin/pacman /usr/bin/pacman-msys2

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/pacman-msys2/lib64

# Put symlinks to native CMake/Make/Ninja/Meson for clang64 platform
RUN mkdir -p /clang64/bin && \
    ln -s /opt/clang-win/bin/cmake /clang64/bin/cmake && \
    ln -s /opt/clang-win/bin/make /clang64/bin/make && \
    ln -s /opt/clang-win/bin/ninja /clang64/bin/ninja && \
    ln -s /usr/bin/meson /clang64/bin/meson

# Run Dropbear SSH server without authentication ('-0' option mod)
ENTRYPOINT [ "dropbear", "-0", "-F", "-s", "-e", "-E", "-p", "22221" ]

EXPOSE 22221
