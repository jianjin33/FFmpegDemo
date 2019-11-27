//
// Created by 简祖明 on 19/11/25.
// 第三版 mp4文件音频视频的播放，对前两版本的代码封装和整理
//

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libyuv.h"
}

#define TAG "native_ffmpeg3"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)

// nb_streams:音频流，视频流，字幕
#define MAX_STREAM 2

struct Player {
    AVFormatContext *formatCtx;

    /**
     * 解码器上下文数组
     */
    AVCodecContext *codecCtx[MAX_STREAM];

    /**
    * 音频和视频索引
    */
    int audioStreamIndex, videoStreamIndex;

    /**
     * 解码线程ID
     */
    pthread_t decodeThreads[MAX_STREAM];

    /**
     * 原生绘制
     */
    ANativeWindow *nativeWindow;
};


/**
 * 第一步：初始化封装格式上下文，获取音视频的索引
 * @param player
 * @param path
 * @return
 */
extern "C" int initAvFromatCtx(struct Player *player, const char *path) {

    avformat_network_init(); // 网络相关
    AVFormatContext *formatCtx = avformat_alloc_context();    // 注意：程序结束调用 avformat_close_input
    if (!formatCtx) {
        return -1;
    }

    player->formatCtx = formatCtx;

    LOGI("init av format context");
    if (avformat_open_input(&formatCtx, path, NULL, NULL) != 0) {
        LOGE("获取视频文件失败！");
        return -1;
    }

    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        LOGE("获取视频文件信息失败！");
        return -1;
    }
    // nb_streams ：输入视频的AVStream个数
    // streams ：输入视频的AVStream []数组
    for (int i = 0; i != formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            player->videoStreamIndex = i;
        } else if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            player->audioStreamIndex = i;
        }
    }

    av_dump_format(formatCtx, 0, path, 0);

    LOGI("init av format context success");
    return 0;
}

/**
 * 初始化解码器上下文
 * @param player
 * @param streamIdx
 */
extern "C" int initCodecCtx(struct Player *player, int streamIdx) {
    // 获取音频或视频解码器
    LOGI("init av codec context");
    AVCodecParameters *codecParameters = player->formatCtx->streams[streamIdx]->codecpar;
    AVCodecContext *codecCtx = avcodec_alloc_context3(NULL);
    if (avcodec_parameters_to_context(codecCtx, codecParameters) < 0) {
        LOGE("通过AVCodecParameters生成AvCodecContext失败！");
        return -1;
    }

    if (codecCtx == NULL) {
        LOGE("AVCodecContext初始化失败");
        return -1;
    }

    // 查找解码器
    AVCodec *codec = avcodec_find_decoder(codecCtx->codec_id);
    if (codec == NULL) {
        LOGE("找不到相关解析器");
        return -1;
    }

    // 打开解码器
    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        LOGE("解析器打开失败");
        return -1;
    }

    player->codecCtx[streamIdx] = codecCtx;

    LOGI("init av codec context success; nb:%d", streamIdx);
    return 0;

}

/**
 * 初始化NativeWindow(获取NativeWindow指针的这步操作我尝试放在 AVPacket分配内存 之后会崩溃)
 */
extern "C" void decodeVideoPrepare(JNIEnv *env, struct Player *player, jobject surface) {
    player->nativeWindow = ANativeWindow_fromSurface(env, surface);
}

/**
 * 视频解码 渲染
 */
extern "C" void
decodeVideo(struct Player *player, AVPacket *packet, ANativeWindow_Buffer *nativeWindowBuffer,
            AVFrame *frameYUV, AVFrame *frameRGB) {

    AVCodecContext *codecCtx = player->codecCtx[player->videoStreamIndex];
    ANativeWindow *nativeWindow = player->nativeWindow;

    int ret, gotPicture;
    // 解码AVPacket->AVFrame
    ret = avcodec_send_packet(codecCtx, packet);
    gotPicture = avcodec_receive_frame(codecCtx, frameYUV);

    if (ret != 0) {
        return;
    }
    if (gotPicture == 0) {
        // 设置缓冲区属性 锁定window
        ANativeWindow_setBuffersGeometry(nativeWindow,
                                         codecCtx->width, codecCtx->height,
                                         WINDOW_FORMAT_RGBA_8888);
        // lock后得到ANativeWindow_Buffer指针
        ANativeWindow_lock(nativeWindow, nativeWindowBuffer, NULL);

        /*  dst_data[4]：        [out]对申请的内存格式化为三个通道后，分别保存其地址
           dst_linesize[4]:        [out]格式化的内存的步长（即内存对齐后的宽度)
           src:        [in]av_alloc()函数申请的内存地址。和ANativeWindow_Buffer->bits是同一块内存
            pix_fmt:    [in] 申请 src内存时的像素格式
            width:        [in]申请src内存时指定的宽度
            height:        [in]申请scr内存时指定的高度
            align:        [in]申请src内存时指定的对齐字节数。*/
        av_image_fill_arrays(frameRGB->data, frameRGB->linesize,
                             static_cast<const uint8_t *>(nativeWindowBuffer->bits),
                             AV_PIX_FMT_ABGR,
                             codecCtx->width,
                             codecCtx->height, 1);

        // 通过yuvlib库将yuv转为rgb
        libyuv::I420ToARGB(frameYUV->data[0], frameYUV->linesize[0],
                           frameYUV->data[1], frameYUV->linesize[1],
                           frameYUV->data[2], frameYUV->linesize[2],
                           frameRGB->data[0], frameRGB->linesize[0],
                           codecCtx->width, codecCtx->height);

        // unlock后会在window上绘制缓冲区rgb内容
        ANativeWindow_unlockAndPost(nativeWindow);
        usleep(1000 * 16);
        av_packet_unref(packet);
    }
}

/**
 * 解码子线程函数
 */
extern "C" void *decodeDataTask(void *arg) {
    struct Player *player = (struct Player *) arg;
    AVFormatContext *formatCtx = player->formatCtx;
    // 编码数据
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));

    ANativeWindow_Buffer *nativeWindowBuffer = new ANativeWindow_Buffer();

    // 分配frame内存空间，注意：程序结束调用 av_frame_free
    AVFrame *frameYUV = av_frame_alloc();
    AVFrame *frameRGB = av_frame_alloc();

    // 压缩的视频数据AVPacket
    int frameCount = 0;
    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == player->videoStreamIndex) {
            decodeVideo(player, packet, nativeWindowBuffer, frameYUV, frameRGB);
            LOGD("视频解码第%d帧", frameCount++);
        }
        av_packet_unref(packet); // important
    }

    av_frame_free(&frameYUV);
    av_frame_free(&frameRGB);
    delete (nativeWindowBuffer);
    return (void *) 0;
}



extern "C"
JNIEXPORT jint JNICALL
Java_com_jianjin33_ffmpeg_v3_FFmpeg_render(JNIEnv *env, jobject instance, jstring path_,
                                           jobject surface) {
    const char *path = env->GetStringUTFChars(path_, NULL);
    struct Player *player = (struct Player *) malloc(sizeof(struct Player));

    //初始化封装格式上下文
    initAvFromatCtx(player, path);
    int videoStreamIndex = player->videoStreamIndex;
    int audioStreamIndex = player->audioStreamIndex;
    //获取音视频解码器，并打开
    initCodecCtx(player, videoStreamIndex);
    initCodecCtx(player, audioStreamIndex);

    decodeVideoPrepare(env, player, surface);

    // 创建子线程解码
    pthread_create(&(player->decodeThreads[videoStreamIndex]), NULL, decodeDataTask,
                   (void *) player);
    return 0;
}
