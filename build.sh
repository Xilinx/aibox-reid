#! /bin/sh

sdkdir=${1}
conf=${2:-Release}

unset LD_LIBRARY_PATH;
source ${sdkdir}/environment-setup-cortexa72-cortexa53-xilinx-linux;
#cd reidtracker/
#./cmake.sh --type=release 
#cd ..
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=${conf} -DCMAKE_TOOLCHAIN_FILE=${sdkdir}/sysroots/x86_64-petalinux-linux/usr/share/cmake/OEToolchainConfig.cmake ../ && make -j && make package
cd ..
