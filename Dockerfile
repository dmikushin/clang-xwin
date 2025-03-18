FROM ubuntu:22.04

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
    apt install -y --no-install-recommends git cmake ninja-build llvm-${LLVM_VERSION} clang-${LLVM_VERSION} clang-tools-${LLVM_VERSION} lld-${LLVM_VERSION} libomp-${LLVM_VERSION}-dev && \
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
    update-alternatives --install /usr/bin/ld ld /usr/bin/ld.lld-${LLVM_VERSION} 100

ARG xwin_version="0.6.6-rc.2-superewald"
ARG xwin_suffix="x86_64-unknown-linux-musl"
ARG xwin_base_url="https://github.com/Jake-Shadle/xwin/releases/download"

COPY xwin-${xwin_version}-${xwin_suffix}.tar.gz . 

# Install the Windows headers and libraries provided by XWin project
# curl --fail -L ${xwin_base_url}/${xwin_version}/xwin-${xwin_version}-${xwin_suffix}.tar.gz | tar -xz && \
RUN set -eux; \
    xwin_url="https://github.com/Jake-Shadle/xwin/releases/download"; \
    cat xwin-${xwin_version}-${xwin_suffix}.tar.gz | tar -xz && \
    cd xwin-${xwin_version}-${xwin_suffix} && \
    XWIN_ACCEPT_LICENSE=true ./xwin --include-atl --include-mfc --include-debug-runtime splat --include-debug-libs && \
    mv .xwin-cache/splat /opt/xwin && \
    cd .. && \
    rm -rf xwin*

WORKDIR /opt/clang-win/bin

COPY cc .
COPY c++ .
COPY llvm-rc .
COPY cmake .

ENV PATH "/opt/clang-win/bin:$PATH"

ENV LDFLAGS "-L/opt/xwin/sdk/Lib/um/x86_64 -L/opt/xwin/crt/lib/x86_64 -L/opt/xwin/sdk/lib/ucrt/x86_64 -L/usr/x86_64-w64-mingw32/lib"

