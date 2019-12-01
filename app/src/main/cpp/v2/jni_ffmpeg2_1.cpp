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
#include "libswresample/swresample.h"
#include "libyuv.h"
}

#define TAG "native_ffmpeg3"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)

#define MAX_AUDIO_FRAME_SIZE 48000 * 4
// nb_streams:音频流，视频流，字幕
#define MAX_STREAM 2

struct Player {
    JavaVM *javaVM;

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

    SwrContext *swrContext;
    /**
     * 输入输出的采样格式
     */
    AVSampleFormat inSampleFormat;
    AVSampleFormat outSampleFormat;

    /**
     * 输入输出的采样率
     */
    int inSampleRate;
    int outSampleRate;

    /**
     * 输出的声道个数
     */
    int outChannelNb;

    jobject audioTrack;
    jmethodID audioTrackWriteMid;
};


/**
 * 第一步：初始化封装格式上下文，获取音视频的索引
 * @param player
 * @param path
 * @return
 */

extern "C" int InitAvFormatCtx(struct Player *player, const char *path) {

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
extern "C" int InitCodecCtx(struct Player *player, int streamIdx) {
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
extern "C" void DecodeVideoPrepare(JNIEnv *env, struct Player *player, jobject surface) {
    player->nativeWindow = ANativeWindow_fromSurface(env, surface);
}

/**
 * 视频解码 渲染
 */
extern "C" void
DecodeVideo(struct Player *player, AVPacket *packet, ANativeWindow_Buffer *nativeWindowBuffer,
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
extern "C" void *DecodeVideoDataTask(void *arg) {
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
            DecodeVideo(player, packet, nativeWindowBuffer, frameYUV, frameRGB);
            LOGD("视频解码第%d帧", frameCount++);
        }
        av_packet_unref(packet); // important
    }

    av_frame_free(&frameYUV);
    av_frame_free(&frameRGB);
    delete (nativeWindowBuffer);
    return (void *) 0;
}


void JNIAudioPrepare(JNIEnv *pEnv, jobject instance, Player *pPlayer) {
    jclass playerClass = pEnv->GetObjectClass(instance);

    //AudioTrack对象
    jmethodID createAudioTrackMid = pEnv->GetMethodID(playerClass, "createAudioTrack",
                                                      "(II)Landroid/media/AudioTrack;");
    jobject audioTrack = pEnv->CallObjectMethod(instance, createAudioTrackMid,
                                                pPlayer->outSampleRate, pPlayer->outChannelNb);

    //调用AudioTrack.play方法
    jclass audioTrackClass = pEnv->GetObjectClass(audioTrack);
    jmethodID audioTrackPlayMid = pEnv->GetMethodID(audioTrackClass, "play", "()V");
    pEnv->CallVoidMethod(audioTrack, audioTrackPlayMid);

    //AudioTrack.write
    jmethodID audioTrackWriteMid = pEnv->GetMethodID(audioTrackClass, "write",
                                                     "([BII)I");
    //JNI end

    // 设置为全局引用，否则子线程调用报错
    pPlayer->audioTrack = pEnv->NewGlobalRef(audioTrack);
    pPlayer->audioTrackWriteMid = audioTrackWriteMid;
}


/**
* 音频解码准备
*/
void DecodeAudioPrepare(Player *player) {
    AVCodecContext *codec_ctx = player->codecCtx[player->audioStreamIndex];

    //重采样设置参数-------------start
    //输入的采样格式
    enum AVSampleFormat inSampleFormat = codec_ctx->sample_fmt;
    //输出采样格式16bit PCM
    enum AVSampleFormat outSampleFormat = AV_SAMPLE_FMT_S16;
    //输入采样率
    int inSampleRate = codec_ctx->sample_rate;
    //输出采样率
    int outSampleRate = inSampleRate;
    //获取输入的声道布局
    //根据声道个数获取默认的声道布局（2个声道，默认立体声stereo）
    //av_get_default_channel_layout(codecCtx->channels);
    uint64_t in_ch_layout = codec_ctx->channel_layout;
    //输出的声道布局（立体声）
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    //frame->16bit 44100 PCM 统一音频采样格式与采样率
    SwrContext *swrContext = swr_alloc();
    swr_alloc_set_opts(swrContext,
                       out_ch_layout, outSampleFormat, outSampleRate,
                       in_ch_layout, inSampleFormat, inSampleRate,
                       0, NULL);
    swr_init(swrContext);

    //输出的声道个数
    int outChannelNb = av_get_channel_layout_nb_channels(out_ch_layout);

    //重采样设置参数-------------end

    player->inSampleFormat = inSampleFormat;
    player->outSampleFormat = outSampleFormat;
    player->inSampleRate = inSampleRate;
    player->outSampleRate = outSampleRate;
    player->outChannelNb = outChannelNb;
    player->swrContext = swrContext;

}

void DecodeAudio(JNIEnv *env, Player *player, AVPacket *packet) {

    AVCodecContext *codecContext = player->codecCtx[player->audioStreamIndex];
    // 解压缩数据
    AVFrame *frame = av_frame_alloc();

    int ret = avcodec_send_packet(codecContext, packet);
    int gotFrame = avcodec_receive_frame(codecContext, frame);

    if (ret != 0) {
        LOGE("解码出错");
        return;
    }

    //16bit 44100 PCM 数据（重采样缓冲区）
    uint8_t *outBuffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE);

    if (gotFrame == 0) {
        swr_convert(player->swrContext, &outBuffer, MAX_AUDIO_FRAME_SIZE,
                    (const uint8_t **) (frame->data), frame->nb_samples);


        int outBufferSize = av_samples_get_buffer_size(NULL, player->outChannelNb,
                                                       frame->nb_samples, player->outSampleFormat,
                                                       1);

        jbyteArray audioSampleArray = env->NewByteArray(outBufferSize);
        jbyte *sampleByte = env->GetByteArrayElements(audioSampleArray, NULL);
        memcpy(sampleByte, outBuffer, outBufferSize);

        // 非常重要的一步  没这步 无数据  同步
        env->ReleaseByteArrayElements(audioSampleArray, sampleByte, 0);

        env->CallIntMethod(player->audioTrack, player->audioTrackWriteMid, audioSampleArray, 0,
                           outBufferSize);
        env->DeleteLocalRef(audioSampleArray);

        usleep(1000 * 16);

    }
    av_frame_free(&frame);
}


/**
 * 解码子线程函数
 */
void *DecodeAudioDataTask(void *arg) {
    struct Player *player = static_cast<Player *>(arg);

    AVFormatContext *formatContext = player->formatCtx;
    AVPacket *packet = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));

    //关联当前线程的JNIEnv
    JavaVM *javaVM = player->javaVM;
    JNIEnv *env;
    javaVM->AttachCurrentThread(&env, NULL);
    int frameCount = 0;
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == player->audioStreamIndex) {
            DecodeAudio(env, player, packet);
            LOGD("audio解码第%d帧", frameCount++);
        }
        av_packet_unref(packet);
    }
    javaVM->DetachCurrentThread();

    return (void *) 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_jianjin33_ffmpeg_FFmpeg_renderV21(JNIEnv *env, jobject instance, jstring path_,
                                            jobject surface) {
    const char *path = env->GetStringUTFChars(path_, NULL);
    Player *player = (Player *) malloc(sizeof(Player));

    JavaVM *javaVM;
    env->GetJavaVM(&javaVM);
    player->javaVM = javaVM;

    // 初始化封装格式上下文
    InitAvFormatCtx(player, path);
    int videoStreamIndex = player->videoStreamIndex;
    int audioStreamIndex = player->audioStreamIndex;
    // 获取音视频解码器，并打开
    InitCodecCtx(player, videoStreamIndex);
    InitCodecCtx(player, audioStreamIndex);

    DecodeVideoPrepare(env, player, surface);
    DecodeAudioPrepare(player);

    // 创建子线程解码
    pthread_create(&(player->decodeThreads[videoStreamIndex]), NULL, DecodeVideoDataTask,
                   (void *) player);

    JNIAudioPrepare(env, instance, player);
    pthread_create(&(player->decodeThreads[audioStreamIndex]), NULL, DecodeAudioDataTask,
                   (void *) player);
    return 0;
}
