//
// Created by 简祖明 on 19/11/25.
// 第三版本视频解码及展示，第三版主要在native层创建线程做处理，并增加暂停和播放按钮
//


#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>


static ANativeWindow *gANativeWindow;
static ANativeWindow_Buffer *gANativeWindowBuffer;

extern "C"
JNIEXPORT jint JNICALL
Java_com_jianjin33_ffmpeg_FFmpeg_render(JNIEnv *env, jobject instance, jstring path_,
                                        jobject jsurface) {

    // 通过java的Surface获得native window
    gANativeWindow = ANativeWindow_fromSurface(env, jsurface);

    return 0;
}

extern "C"
JNIEXPORT jint JNICALL Java_com_jianjin33_ffmpeg_FFmpeg_stop(JNIEnv *env, jobject instance) {

    return 0;
}

extern "C"
JNIEXPORT jint JNICALL Java_com_jianjin33_ffmpeg_FFmpeg_pause(JNIEnv *env, jobject instance) {

    return 0;
}

extern "C"
JNIEXPORT jint JNICALL Java_com_jianjin33_ffmpeg_FFmpeg_resume(JNIEnv *env, jobject instance) {

    return 0;
}