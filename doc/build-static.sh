#!/bin/bash

make clean
# NDK的路径，根据自己的安装位置进行设置
export TMPDIR=$HOME/worksapces/FFmpeg/ffmpeg_install
export NDK=$HOME/environment/android-ndk-r13
export SYSROOT=$NDK/platforms/android-21/arch-arm
export TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64
export ADDI_CFLAGS="-marm"

# arm v7-a
CPU=armv7-a
#PREFIX=./android/$CPU 和下面效果一样
PREFIX=$(pwd)/android/$CPU
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfp -marm -march=$CPU "
ADDITIONAL_CONFIGURE_FLAG=

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
--disable-shared \
--enable-static \
--enable-runtime-cpudetect \
--enable-gpl \
--enable-small \
--enable-cross-compile \
--disable-debug \
--disable-doc \
--disable-asm \
--disable-ffmpeg \
--disable-ffplay \
--disable-avresample \
--disable-postproc \
--disable-network \
--disable-encoders \
--disable-muxers \
--disable-parsers \
--enable-parser=aac \
--enable-parser=h264 \
--disable-decoders \
--enable-decoder=h264 \
--enable-decoder=aac \
--disable-demuxers \
--enable-demuxer=mov \
--enable-demuxer=m4a \
--disable-ffprobe \
--disable-avdevice \
--disable-symver \
--disable-x86asm \
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

$TOOLCHAIN/bin/arm-linux-androideabi-ld \
-rpath-link=$SYSROOT/usr/lib \
-L$SYSROOT/usr/lib \
-L$PREFIX/lib \
-soname libffmpeg.so -shared -nostdlib -Bsymbolic --whole-archive --no-undefined -o \
$PREFIX/libffmpeg.so \
libavcodec/libavcodec.a \
libavfilter/libavfilter.a \
libswresample/libswresample.a \
libavformat/libavformat.a \
libavutil/libavutil.a \
libswscale/libswscale.a \
-lc -lm -lz -ldl -llog --dynamic-linker=/system/bin/linker \
$TOOLCHAIN/lib/gcc/arm-linux-androideabi/4.9.x/libgcc.a \

}

build_one
