FROM ubuntu:22.04 AS clang

LABEL maintainer="dmitry@kernelgen.org"

ARG LLVM_VERSION=20

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL C.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8

# We will use Clang from LLVM compiler toolchain, which is naturally always
# a native compiler and a cross-compiler all in one.
# For example, Clang in Linux could be given an option --target=x86_64-pc-windows-msvc
# to produce a Windows binary (given that the Windows headers and libraries are provided)
RUN set -eux; \
    apt update && \
    apt install -y --no-install-recommends curl wget ca-certificates && \
    echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-${LLVM_VERSION} main" > /etc/apt/sources.list.d/llvm.list && \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc && \
    apt update && \
    apt install -y --no-install-recommends git cmake make ninja-build meson llvm-${LLVM_VERSION} clang-${LLVM_VERSION} clang-tools-${LLVM_VERSION} libc++-${LLVM_VERSION}-dev libc++abi-${LLVM_VERSION}-dev lld-${LLVM_VERSION} libomp-${LLVM_VERSION}-dev libz-dev vim fish tmux mc ssh gpg gpg-agent libgpgme11 libpcre3 libsystemd0 patch libarchive-tools file strace&& \
    ln -s clang-${LLVM_VERSION} /usr/bin/clang && ln -s clang /usr/bin/clang++ && ln -s lld-${LLVM_VERSION} /usr/bin/ld.lld && \
    ln -s clang-cl-${LLVM_VERSION} /usr/bin/clang-cl && ln -s llvm-ar-${LLVM_VERSION} /usr/bin/llvm-lib && ln -s lld-link-${LLVM_VERSION} /usr/bin/lld-link && \
    ln -s llvm-rc-${LLVM_VERSION} /usr/bin/llvm-rc && \
    clang-${LLVM_VERSION} -v && \
    clang++-${LLVM_VERSION} -v && \
    ld.lld-${LLVM_VERSION} -v && \
    llvm-lib -v && \
    clang-cl -v && \
    lld-link --version && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/clang-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/ld ld /usr/bin/ld.lld-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/ar ar /usr/bin/llvm-ar-${LLVM_VERSION} 100


FROM clang as pacman

RUN set -eux; \
    apt update && \
    apt install -y --no-install-recommends \
        pkg-config libarchive-dev libssl-dev libcurl4-openssl-dev libgpgme-dev \
        libpcre3-dev libsystemd-dev systemd

# We use MSYS2, which is pacman-flavored Windows packages provider that makes libraries
# dependencies installation as easy as in Linux.
RUN set -eux; \
    git clone https://github.com/msys2/msys2-pacman.git && \
    cd msys2-pacman && \
    meson setup builddir --buildtype=release --prefix=/opt/msys2/usr --sysconfdir=/opt/msys2/etc && \
    meson compile -C builddir && \
    meson install -C builddir

# Also install the keyring required for packages verification
RUN set -eux; \
    git clone https://github.com/msys2/MSYS2-keyring.git && \
    cd MSYS2-keyring && \
    sed -i "s|/usr/local|/usr|" Makefile && \
    make DESTDIR=/opt/msys2

# Also include pkgfile to inspect the package contents without installing them
RUN set -eux; \
    git clone https://github.com/falconindy/pkgfile.git && \
    cd pkgfile && \
    sed -i "s|/etc/pacman.conf|/opt/msys2/etc/pacman.conf|g" meson.build && \
    CXXFLAGS="-stdlib=libc++" meson setup builddir --buildtype=release --prefix=/opt/msys2/usr && \
    meson compile -C builddir && \
    meson install -C builddir

WORKDIR /opt/msys2/bin

RUN wget https://raw.githubusercontent.com/msys2/MSYS2-packages/refs/heads/master/pacman/makepkg-mingw

WORKDIR /opt/msys2/etc

RUN rm -rf pacman.conf && \
    wget https://raw.githubusercontent.com/msys2/MSYS2-packages/refs/heads/master/pacman/pacman.conf && \
    sed -i "s|/etc/pacman.d|/opt/msys2/etc/pacman.d|g" pacman.conf && \
    sed -i "s|CheckSpace|#CheckSpace|g" pacman.conf && \
    wget https://raw.githubusercontent.com/msys2/MSYS2-packages/refs/heads/master/pacman/makepkg_mingw.conf

WORKDIR /opt/msys2/etc/pacman.d

RUN wget https://raw.githubusercontent.com/msys2/MSYS2-packages/refs/heads/master/pacman-mirrors/mirrorlist.msys && \
    wget https://raw.githubusercontent.com/msys2/MSYS2-packages/refs/heads/master/pacman-mirrors/mirrorlist.mingw

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
    xwin_url="https://github.com/Jake-Shadle/xwin/releases/download"; \
    curl --fail -L ${xwin_base_url}/${xwin_version}/xwin-${xwin_version}-${xwin_suffix}.tar.gz | tar -xz && \
    cd xwin-${xwin_version}-${xwin_suffix} && \
    XWIN_ACCEPT_LICENSE=true ./xwin ${INCLUDE_ATL} ${INCLUDE_MFC} --include-debug-runtime splat --include-debug-libs && \
    mv .xwin-cache/splat /opt/xwin && \
    cd .. && \
    rm -rf xwin*

WORKDIR /opt/clang-win/bin

COPY cc .
COPY c++ .
COPY llvm-rc .
COPY make .
COPY cmake .
COPY ninja .

ENV PATH "/opt/clang-win/bin:$PATH"

ENV LDFLAGS "-L/opt/xwin/sdk/Lib/um/x86_64 -L/opt/xwin/crt/lib/x86_64 -L/opt/xwin/sdk/lib/ucrt/x86_64 -L/usr/x86_64-w64-mingw32/lib"

COPY insensitive.cpp .

RUN clang++-${LLVM_VERSION} -O3 -std=c++17 -fPIC insensitive.cpp -shared -o libinsensitive.so && \
    rm -rf insensitive.cpp

ENV CLICOLOR_FORCE 1

ENV SHELL /usr/bin/fish

#ENV INSENSITIVE_DEBUG 1

#ENV INSENSITIVE_DEBUG_LEVEL TRACE

ENV DOCKER 1

RUN chsh -s /usr/bin/fish root

RUN bash -c 'echo -e "# /etc/shells: always use fish as a login shell\n/usr/bin/fish\n/bin/bash" >/etc/shells'

# Pre-generate Dropbear SSH keys
RUN mkdir -p /etc/dropbear && \
    dropbearkey -t rsa -f /etc/dropbear/dropbear_rsa_host_key && \
    dropbearkey -t ed25519 -f /etc/dropbear/dropbear_ed25519_host_key && \
    dropbearkey -t ecdsa -f /etc/dropbear/dropbear_ecdsa_host_key

# Install MSYS2 pacman and configure it for fetching MSYS2 packages.
# This way we can bring the entire opensource package universe together
# with our Clang-based Windows toolchain.
COPY --from=pacman /opt/msys2 /opt/msys2

ENV PATH=$PATH:/opt/msys2/usr/bin
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/msys2/usr/lib64

# Initialize pacman
RUN pacman-key --init && \
    pacman-key --populate msys2 && \
    mkdir -p /var/lib/pacman && \
    mkdir -p /var/cache/pkgfile && \
    pacman -Syu --noconfirm && \
    pacman -Sy --noconfirm msys2-keyring

# Put symlinks to native CMake/Make/Ninja/Meson for clang64 platform
# TODO: After moving the container from Ubuntu to CachyOS, we should
# use overlay to put all native executables and libraries on top of
# the /opt/xwin target
RUN mkdir -p /clang64/bin && \
    ln -s /opt/clang-win/bin/cmake /clang64/bin/cmake && \
    ln -s /opt/clang-win/bin/make /clang64/bin/make && \
    ln -s /opt/clang-win/bin/ninja /clang64/bin/ninja && \
    ln -s /opt/clang-win/bin/meson /clang64/bin/meson

# Run Dropbear SSH server without authentication ('-0' option mod)
ENTRYPOINT [ "dropbear", "-0", "-F", "-s", "-e", "-E", "-p", "22221" ]

EXPOSE 22221
