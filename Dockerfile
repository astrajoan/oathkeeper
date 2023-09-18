FROM ubuntu:23.04

SHELL ["/bin/bash", "-c"]
WORKDIR /root

# install dependencies
RUN set -e; \
    apt-get update; \
    apt-get upgrade -y; \
    apt-get install -y build-essential autoconf libtool pkg-config git wget gcc-13 g++-13 libssl-dev; \
    apt-get autoremove; \
    apt-get clean; \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 60 \
        --slave /usr/bin/g++ g++ /usr/bin/g++-13 \
        --slave /usr/bin/gcov gcov /usr/bin/gcov-13 \
        --slave /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-13 \
        --slave /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-13 \
        --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-13 \
        --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-13 \
        --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-13; \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 50 \
        --slave /usr/bin/g++ g++ /usr/bin/g++-12 \
        --slave /usr/bin/gcov gcov /usr/bin/gcov-12 \
        --slave /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-12 \
        --slave /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-12 \
        --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-12 \
        --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-12 \
        --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-12

# cmake 3.27
RUN set -e; \
    wget https://github.com/Kitware/CMake/releases/download/v3.27.4/cmake-3.27.4.tar.gz; \
    tar xzf cmake-3.27.4.tar.gz; \
    pushd cmake-3.27.4; \
    ./bootstrap; \
    make -j$(nproc); \
    make install; \
    popd; \
    rm -rf cmake-3.27.4.tar.gz cmake-3.27.4

# gflags
RUN set -e; \
    git clone https://github.com/gflags/gflags.git; \
    pushd gflags; \
    cmake -DBUILD_SHARED_LIBS=ON -B build -S .; \
    cmake --build build --target install; \
    popd; \
    rm -rf gflags

# glog
RUN set -e; \
    git clone https://github.com/google/glog.git; \
    pushd glog; \
    cmake -B build -S .; \
    cmake --build build --target install; \
    popd; \
    rm -rf glog

# googletest
RUN set -e; \
    git clone https://github.com/google/googletest.git; \
    pushd googletest; \
    cmake -B build -S .; \
    cmake --build build --target install; \
    popd; \
    rm -rf googletest

# boost 1.82
RUN set -e; \
    wget https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz; \
    tar xzf boost_1_82_0.tar.gz; \
    pushd boost_1_82_0; \
    ./bootstrap.sh; \
    ./b2 -j$(nproc); \
    ./b2 install; \
    popd; \
    rm -rf boost_1_82_0.tar.gz boost_1_82_0

# grpc
RUN set -e; \
    git clone --recurse-submodules -b v1.57.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc; \
    mkdir -p grpc/cmake/build; \
    pushd grpc/cmake/build; \
    cmake -DgRPC_INSTALL=ON ../..; \
    make -j$(nproc); \
    make install; \
    popd; \
    rm -rf grpc

# asio-grpc
RUN set -e; \
    git clone https://github.com/Tradias/asio-grpc.git; \
    pushd asio-grpc; \
    cmake -B build -S .; \
    cmake --build build --target install; \
    popd; \
    rm -rf asio-grpc
