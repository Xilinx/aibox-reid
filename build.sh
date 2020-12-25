#! /bin/sh

sdkdir=/group/xbjlab/dphi_software/software/workspace/xysheng/som/opt/petalinux/2020.2/
conf=${1:-Release}
unset LD_LIBRARY_PATH;
export CPLUS_INCLUDE_PATH=/group/xbjlab/dphi_software/software/workspace/xysheng/som/opt/petalinux/2020.2/sysroots/aarch64-xilinx-linux/install/Release/include/:$CPLUS_INCLUDE_PATH
export LD_LIBRARY_PATH=/group/xbjlab/dphi_software/software/workspace/xysheng/som/opt/petalinux/2020.2/sysroots/aarch64-xilinx-linux/install/Release/lib/:$LD_LIBRARY_PATH
source ${sdkdir}/environment-setup-aarch64-xilinx-linux;
cd reidtracker/
cmake.sh --type=release 
cd ..
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=${conf} -DCMAKE_TOOLCHAIN_FILE=${sdkdir}/sysroots/x86_64-petalinux-linux/usr/share/cmake/OEToolchainConfig.cmake ../ && make -j && make package
make install
cd ..
