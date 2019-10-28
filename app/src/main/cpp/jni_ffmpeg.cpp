//
// Created by 简祖明 on 19/10/28.
//
#include <jni.h>
#include <android/log.h>
#include <string>


extern "C"
{
//#include "include/libavformat/avformat.h"
}

#define TAG "native_ffmpeg"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)

extern "C" JNIEXPORT jstring

JNICALL
Java_com_jianjin33_ffmpeg_FFmpeg_init(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "init";
    return env->NewStringUTF(hello.c_str());
}