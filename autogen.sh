#!/bin/bash

install_prefix=$PWD

cd "$(dirname $0)"

echo "Clearing build data"
rm -r build
rm -r bin
mkdir build
cd build

BUILD_MODE=${BUILD_MODE:-Release}

echo "Passing '$@' argumnets "
echo "Build type = ${BUILD_MODE}"

cmake ../                                   \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON      \
    -DCMAKE_INSTALL_PREFIX=$install_prefix  \
    -DCMAKE_BUILD_TYPE=$BUILD_MODE          \
    "$@"

make install -j64
