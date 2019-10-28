#!/bin/sh

make clean
# NDK的路径，根据自己的安装位置进行设置
export TMPDIR=$HOME/worksapces/FFmpeg/ffmpeg_install
export NDK=$HOME/environment/android-ndk-r13
export SYSROOT=$NDK/platforms/android-21/arch-arm/
export TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64
export CPU=arm
export PREFIX=$(pwd)/android/$CPU
export ADDI_CFLAGS="-marm"


#--disable-decoders \
#--disable-encoders \

function build_one
{
./configure \
--prefix=$PREFIX \
--target-os=linux \
--cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
--arch=arm \
--sysroot=$SYSROOT \
--extra-cflags="-Os -fpic $ADDI_CFLAGS" \
--extra-ldflags="$ADDI_LDFLAGS" \
--cc=$TOOLCHAIN/bin/arm-linux-androideabi-gcc \
--nm=$TOOLCHAIN/bin/arm-linux-androideabi-nm \
--enable-shared \
--enable-runtime-cpudetect \
--enable-gpl \
--enable-small \
--enable-cross-compile \
--disable-debug \
--disable-static \
--disable-doc \
--disable-asm \
--disable-ffmpeg \
--disable-ffplay \
--disable-network \
--disable-avdevice \
--disable-avresample \
--disable-postproc \
--disable-encoders \
--disable-muxers \
--disable-ffprobe \
--disable-symver \
--disable-stripping \
$ADDITIONAL_CONFIGURE_FLAG
sed -i '' 's/HAVE_LRINT 0/HAVE_LRINT 1/g' config.h
sed -i '' 's/HAVE_LRINTF 0/HAVE_LRINTF 1/g' config.h
sed -i '' 's/HAVE_ROUND 0/HAVE_ROUND 1/g' config.h
sed -i '' 's/HAVE_ROUNDF 0/HAVE_ROUNDF 1/g' config.h
sed -i '' 's/HAVE_TRUNC 0/HAVE_TRUNC 1/g' config.h
sed -i '' 's/HAVE_TRUNCF 0/HAVE_TRUNCF 1/g' config.h
sed -i '' 's/HAVE_CBRT 0/HAVE_CBRT 1/g' config.h
sed -i '' 's/HAVE_RINT 0/HAVE_RINT 1/g' config.h
make clean
make -j4
make install
}
build_one
