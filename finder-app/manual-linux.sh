#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
PROJECT_HOME=/home/sundar/aesd/as3/assignment-3-SundarKrishnakumar

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    
    # TODO: Add your kernel build steps here
    # Deep clean the kernel build tree
    echo "Deep clean the kernel build tree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    # create config file using defconfig. We are configurin for "virt" arm dev board
    # !!!NOTE: Make sure to install openssl, bison and flex before this step.
    echo "create config file using defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # build the kernel image to boot with QEMU
    echo "build the kernel image"
    make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

    echo "Adding the Image in outdir"
    cp ./arch/${ARCH}/boot/Image ${OUTDIR}/


fi 


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
# cteate rootfs directory
echo "creating rootfs directory"
mkdir ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs

# Populate the directory tree under rootfs
echo "Populating the directory tree under rootfs"
mkdir bin dev etc home lib proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "Configuring busybox"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

else
    cd busybox
fi

# TODO: Make and insatll busybox
echo "Compiling and installing busybox"
# cross compile
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

# install
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"


# TODO: Add library dependencies to rootfs
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
mkdir lib64
# cp -a ${SYSROOT}/lib64/ld-2.33.so lib64
# cp -a ${SYSROOT}/lib64/libc-2.33.so lib64
# cp -a ${SYSROOT}/lib64/libm-2.33.so lib64
# cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 lib
# cp -a ${SYSROOT}/lib64/libc.so.6 lib64
# cp -a ${SYSROOT}/lib64/libm.so.6 lib64
# cp -a ${SYSROOT}/lib64/libresolv.so.2 lib64
# cp -a ${SYSROOT}/lib64/libresolv-2.33.so lib64

cp -L $SYSROOT/lib/ld-linux-aarch64.* lib
cp -L $SYSROOT/lib64/libm.so.* lib64
cp -L $SYSROOT/lib64/libresolv.so.* lib64
cp -L $SYSROOT/lib64/libc.so.* lib64



# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1 

# TODO: Clean and build the writer utility
cd ${PROJECT_HOME}/finder-app/
make  clean
make CROSS_COMPILE=${CROSS_COMPILE} 


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${PROJECT_HOME}/finder-app/finder-test.sh ${OUTDIR}/rootfs/home
cp ${PROJECT_HOME}/finder-app/conf/ -r ${OUTDIR}/rootfs/home
cp ${PROJECT_HOME}//finder-app/finder.sh ${OUTDIR}/rootfs/home
cp ${PROJECT_HOME}/finder-app/writer ${OUTDIR}/rootfs/home
cp ${PROJECT_HOME}/finder-app/autorun-qemu.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *


# TODO: Create initramfs.cpio.gz
# Do sudo apt-get install u-boot-tools to install cpio and mkImage before running them
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
rm -f initramfs.cpio.gz
gzip initramfs.cpio 