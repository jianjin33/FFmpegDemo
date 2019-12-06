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
#include "queue.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "libyuv.h"
#include <libavutil/time.h>
}

#define TAG "native_ffmpeg3"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)

#define MAX_AUDIO_FRAME_SIZE 48000 * 4
// nb_streams:音频流，视频流，字幕
#define MAX_STREAM 2
#define PACKET_QUEUE_SIZE 50
#define MIN_SLEEP_TIME_US 1000ll
#define AUDIO_TIME_ADJUST_US -200000ll

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
     * 流的总个数
     */
    int nbStreams;

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

    /**
    * 解码线程ID
    */
    pthread_t threadReadFromStream;

    BufferQueue *bufferQueue;

    Queue *packets[MAX_STREAM];

    /**
     * 视频开始时间
     */
    int64_t startTime;

    int64_t audioLock;

    pthread_mutex_t mutex;

    pthread_cond_t cond;
};

/**
 * 解码数据
 */
struct DecodeData {
    Player *player;
    int streamIndex;
};


/**
 * 获取视频当前播放时间
 */
int64_t GetPlayerCurrentVideoTime(Player *player) {
    int64_t current_time = av_gettime();
    return current_time - player->startTime;
}

/**
 * 延迟
 */
void PlayerWaitForFrame(Player *player, int64_t streamTime,
                        int nbStream) {
    pthread_mutex_lock(&player->mutex);
    for (;;) {
        int64_t current_video_time = GetPlayerCurrentVideoTime(player);
        int64_t sleepTime = streamTime - current_video_time;
        if (sleepTime < -300000ll) {
            // 300 ms late
            int64_t new_value = player->startTime - sleepTime;
            LOGI("player_wait_for_frame[%d] correcting %f to %f because late",
                 nbStream, (av_gettime() - player->startTime) / 1000000.0,
                 (av_gettime() - new_value) / 1000000.0);

            player->startTime = new_value;
            pthread_cond_broadcast(&player->cond);
        }

        if (sleepTime <= MIN_SLEEP_TIME_US) {
            // We do not need to wait if time is slower then minimal sleep time
            break;
        }

        if (sleepTime > 500000ll) {
            // if sleep time is bigger then 500ms just sleep this 500ms
            // and check everything again
            sleepTime = 500000ll;
        }
        //等待指定时长
        struct timespec time = {sleepTime / 1000ll};
        pthread_cond_timedwait(&player->cond, &player->mutex, &time);
    }
    pthread_mutex_unlock(&player->mutex);
}



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

    player->nbStreams = formatCtx->nb_streams;
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
 * 解码渲染视频
 */
extern "C" void *DecodeVideo(Player *player, AVPacket *packet) {
    ANativeWindow_Buffer buffer, *nativeWindowBuffer = &buffer;

    // 分配frame内存空间，注意：程序结束调用 av_frame_free
    AVFrame *frameYUV = av_frame_alloc();
    AVFrame *frameRGB = av_frame_alloc();

    AVCodecContext *codecCtx = player->codecCtx[player->videoStreamIndex];
    ANativeWindow *nativeWindow = player->nativeWindow;

    int gotPicture;
    // 解码AVPacket->AVFrame
    int ret = avcodec_send_packet(codecCtx, packet);
    gotPicture = avcodec_receive_frame(codecCtx, frameYUV);

    if (ret != 0) {
        return NULL;
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

        //计算延迟
        int64_t pts = av_frame_get_best_effort_timestamp(frameYUV);
        //转换（不同时间基时间转换）
        AVStream *stream = player->formatCtx->streams[player->videoStreamIndex];
        int64_t time = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);

        PlayerWaitForFrame(player, time, player->videoStreamIndex);

        // unlock后会在window上绘制缓冲区rgb内容
        ANativeWindow_unlockAndPost(nativeWindow);
        usleep(1000 * 16);
        av_packet_unref(packet);
    }


    av_frame_free(&frameYUV);
    av_frame_free(&frameRGB);
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

        AVStream *stream = player->formatCtx->streams[player->audioStreamIndex];
        int64_t pts = packet->pts;
        if (pts != AV_NOPTS_VALUE) {
            player->audioLock = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);
            LOGI("player_write_audio - read from pts");
            PlayerWaitForFrame(player,
                               player->audioLock + AUDIO_TIME_ADJUST_US, player->audioStreamIndex);
        }

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
 * 给AVPacket开辟空间，后面会将AVPacket栈内存数据拷贝至这里开辟的空间
 */
void *playerFillPacket() {
    //请参照我在vs中写的代码
    AVPacket *packet = static_cast<AVPacket *>(malloc(sizeof(AVPacket)));
    return packet;
}


void PlayerAllocQueues(Player *player) {
    for (int i = 0; i < player->nbStreams; ++i) {
        Queue *queue = player->bufferQueue->QueueInit(PACKET_QUEUE_SIZE, playerFillPacket);
        player->packets[i] = queue;
    }
}


void *PacketFreeFunc(AVPacket *packet) {
    av_packet_unref(packet);
    return 0;
}

/**
 * 读取视频文件中AVPacket，分别放入两个队列中
 * @param player
 * @return
 */
void *PlayReadFromStream(void *arg) {
    Player *player = static_cast<Player *>(arg);

    int ret;
    // 栈内存保存AvPacket，不需要回收处理。
    AVPacket avPacket, *packet = &avPacket;

    for (;;) {
        ret = av_read_frame(player->formatCtx, packet);

        if (ret < 0) {
            LOGE("文件结束或出错");
            break;
        }

        Queue *queue = player->packets[packet->stream_index];

        pthread_mutex_lock(&player->mutex);

        // AvPacket放入队列
        AVPacket *packetData = static_cast<AVPacket *>(player->bufferQueue->QueuePush(queue,
                                                                                      &player->mutex,
                                                                                      &player->cond));
        pthread_mutex_unlock(&player->mutex);


        // 拷贝（间接赋值，拷贝结构体数据）
        *packetData = avPacket;
    }

    return (void *) 0;

}

void *DecodeDataTask(void *arg) {
    DecodeData *decodeData = static_cast<DecodeData *>(arg);
    Player *player = decodeData->player;
    int streamIndex = decodeData->streamIndex;

    Queue *queue = player->packets[streamIndex];
    BufferQueue *bufferQueue = player->bufferQueue;

    // 消费者
    int videoFrameCount = 0, audioFrameCount = 0;


    JavaVM *javaVM = player->javaVM;
    JNIEnv *env;
    javaVM->AttachCurrentThread(&env, NULL);

    for (;;) {
        //消费AVPacket

        pthread_mutex_lock(&player->mutex);
        AVPacket *packet = (AVPacket *) bufferQueue->QueuePop(queue, &player->mutex,
                                                              &player->cond);
        pthread_mutex_unlock(&player->mutex);

        if (streamIndex == player->videoStreamIndex) {
            DecodeVideo(player, packet);
            LOGI("视频编码:%d", videoFrameCount++);
        } else if (streamIndex == player->audioStreamIndex) {
            DecodeAudio(env, player, packet);
            LOGI("音频编码:%d", audioFrameCount++);
        }

    }
    javaVM->DetachCurrentThread();
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_jianjin33_ffmpeg_v3_FFmpeg3_render(JNIEnv *env, jobject instance, jstring path_,
                                            jobject surface) {
    const char *path = env->GetStringUTFChars(path_, NULL);
    Player *player = (Player *) malloc(sizeof(Player));

    JavaVM *javaVM;
    env->GetJavaVM(&javaVM);
    player->javaVM = javaVM;

    // 栈内存创建该对象 注意 BufferQueue bufferQueue()编译和运行都没有问题，但是并没有创建对象
    BufferQueue bufferQueue;
    player->bufferQueue = &bufferQueue;

    // 初始化封装格式上下文
    InitAvFormatCtx(player, path);
    int videoStreamIndex = player->videoStreamIndex;
    int audioStreamIndex = player->audioStreamIndex;
    // 获取音视频解码器，并打开
    InitCodecCtx(player, videoStreamIndex);
    InitCodecCtx(player, audioStreamIndex);

    DecodeVideoPrepare(env, player, surface);
    DecodeAudioPrepare(player);

    JNIAudioPrepare(env, instance, player);
    PlayerAllocQueues(player);

    // 生产者线程
    pthread_create(&(player->threadReadFromStream), NULL, PlayReadFromStream, player);

    sleep(2);
    player->startTime = 0;

    // 消费者线程
    // 初始化视频数据封装的结构体
    DecodeData videoData = {player, videoStreamIndex}, *pVideoDecodeData = &videoData;
    pthread_create(&(player->decodeThreads[videoStreamIndex]), NULL, DecodeDataTask,
                   (void *) pVideoDecodeData);

    DecodeData audioData = {player, audioStreamIndex}, *pAudioDecodeData = &audioData;
    pthread_create(&(player->decodeThreads[audioStreamIndex]), NULL, DecodeDataTask,
                   (void *) pAudioDecodeData);

    pthread_join(player->threadReadFromStream, NULL);
    pthread_join(player->decodeThreads[videoStreamIndex], NULL);
    pthread_join(player->decodeThreads[audioStreamIndex], NULL);

    return 0;
}
