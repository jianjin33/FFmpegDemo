#! /usr/bin/env bash
#
# Copyright (C) 2013-2014 Zhang Rui <bbcallen@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This script is based on projects below
# https://github.com/yixia/FFmpeg-Android
# http://git.videolan.org/?p=vlc-ports/android.git;a=summary

#----------
UNI_BUILD_ROOT=`pwd`

# $0:本身名字，$1第一个参数，$2第二个参数
FF_TARGET=$1
FF_TARGET_EXTRA=$2
# 在"set -e"之后出现的代码，一旦出现了返回值非零，整个脚本就会立即退出
set -e
# 用于脚本调试。set是把它下面的命令打印到屏幕set -x 是开启 set +x是关闭 set -o是查看 (xtrace)
set +x

FF_ACT_ARCHS_32="armv5 armv7a x86"
FF_ACT_ARCHS_64="armv5 armv7a arm64 x86 x86_64"
FF_ACT_ARCHS_ALL=$FF_ACT_ARCHS_64

echo_archs() {
    echo "===================="
    echo "[*] check archs"
    echo "===================="
    echo "FF_ALL_ARCHS = $FF_ACT_ARCHS_ALL"
    echo "FF_ACT_ARCHS = $*"    
    echo ""
}

# $* 和 $@ 都表示传递给函数或脚本的所有参数，不被双引号" "包围时，它们之间没有任何区别，都是将接收到的每个参数看做一份数据，彼此之间以空格来分隔。
#"$*"会将所有的参数从整体上看做一份数据，而不是把每个参数都看做一份数据。
#"$@"仍然将每个参数都看作一份数据，彼此之间是独立的。
#
# $#表示参数个数


echo_usage() {
    echo "Usage:"
    echo "  compile-ffmpeg.sh armv5|armv7a|arm64|x86|x86_64"
    echo "  compile-ffmpeg.sh all|all32"
    echo "  compile-ffmpeg.sh all64"
    echo "  compile-ffmpeg.sh clean"
    echo "  compile-ffmpeg.sh check"
    exit 1
}

echo_nextstep_help() {
    echo ""
    echo "--------------------"
    echo "[*] Finished"
    echo "--------------------"
    echo "# to continue to build ijkplayer, run script below,"
    echo "sh compile-ijk.sh "
}

#---------- 根据参数编译对应平台的ffmpeg（第一个参数） 
# case语法格式
#case var in
#    pattern 1 )
#        … ;;
#    pattern 2 )
#        … ;;
#    *)
#        … ;;
#esac
#
#for语句
#for var in …; do
#    …
#done
#
#for (( cond1; cond2; cond3 )) do
#    …
#done
#



case "$FF_TARGET" in
    "")
        echo_archs armv7a
        sh tools/do-compile-ffmpeg.sh armv7a
    ;;
    armv5|armv7a|arm64|x86|x86_64)
        echo_archs $FF_TARGET $FF_TARGET_EXTRA
        sh tools/do-compile-ffmpeg.sh $FF_TARGET $FF_TARGET_EXTRA
        echo_nextstep_help
    ;;
    all32)
        echo_archs $FF_ACT_ARCHS_32
        for ARCH in $FF_ACT_ARCHS_32
        do
            sh tools/do-compile-ffmpeg.sh $ARCH $FF_TARGET_EXTRA
        done
        echo_nextstep_help
    ;;
    all|all64)
        echo_archs $FF_ACT_ARCHS_64
        for ARCH in $FF_ACT_ARCHS_64
        do
            sh tools/do-compile-ffmpeg.sh $ARCH $FF_TARGET_EXTRA
        done
        echo_nextstep_help
    ;;
    clean)
        echo_archs FF_ACT_ARCHS_64
        for ARCH in $FF_ACT_ARCHS_ALL
        do
            if [ -d ffmpeg-$ARCH ]; then
                cd ffmpeg-$ARCH && git clean -xdf && cd -
            fi
        done
        rm -rf ./build/ffmpeg-*
    ;;
    check)
        echo_archs FF_ACT_ARCHS_ALL
    ;;
    *)
        echo_usage
        exit 1
    ;;
esac
