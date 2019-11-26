//
// Created by 简祖明 on 19/11/25.
//
#define AUDIO_RENDER_H

#include <jni.h>
#include <android/log.h>
#include <iostream>
#include <unistd.h>

extern "C" {
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#define TAG "native_ffmpeg"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)


namespace render {

    int Sound(JNIEnv *env, jobject jthiz,AVFormatContext *pFormatCtx, int audioIndex);
}