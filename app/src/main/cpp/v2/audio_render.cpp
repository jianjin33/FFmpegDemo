//
// Created by 简祖明 on 19/11/25.
//
#include <pthread.h>
#include <unistd.h>
#include "audio_render.h"

const long MAX_AUDIO_FRME_SIZE = 48000 * 4;

using namespace render;

int gAudioIndex;
jobject gInstance;
jclass gFFmpegClass;
AVFormatContext *gFormatCtx;

JavaVM *gJvm;

void *RenderSoundTask(void *arg) {
    JNIEnv *env;
    gJvm->AttachCurrentThread(&env, NULL);
    // 获取解码器
    AVCodecParameters *codecParameters = gFormatCtx->streams[gAudioIndex]->codecpar;
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(NULL);
    if (avcodec_parameters_to_context(pCodecCtx, codecParameters) < 0) {
        LOG_D("通过AVCodecParameters生成AvCodecContext失败！");
        return NULL;
    }

    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        LOG_D("无法获取解码器");
        return NULL;
    }
    // 打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) != 0) {
        LOG_D("无法打开解码器");
        return NULL;
    }
    // 申请AvPacket内存
    AVPacket *packet = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));
    // 解压缩数据
    AVFrame *frame = av_frame_alloc();
    // 统一采样格式和采样率
    SwrContext *pSwrCtx = swr_alloc();

    AVSampleFormat inSampleFmt = pCodecCtx->sample_fmt;
    AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_S16;

    int inSampleRate = pCodecCtx->sample_rate;
    int outSampleRate = inSampleRate;

    uint64_t inChannelLayout = pCodecCtx->channel_layout;
    uint64_t outChannelLayout = AV_CH_LAYOUT_STEREO;

    swr_alloc_set_opts(pSwrCtx,
                       outChannelLayout, outSampleFmt, outSampleRate,
                       inChannelLayout, inSampleFmt, inSampleRate,
                       0, NULL);
    swr_init(pSwrCtx);
    int outChannelNb = av_get_channel_layout_nb_channels(outChannelLayout);

    // 输出声道数据

    // 获取 java对象和方法
    // 获取createAudioTrack方法
    jmethodID createAudioTrackMid = env->GetMethodID(gFFmpegClass, "createAudioTrack",
                                                     "(II)Landroid/media/AudioTrack;");
    // 调用获得AudioTrack对象
    jobject audioTrackInstance = env->CallObjectMethod(gInstance, createAudioTrackMid,
                                                       outSampleRate,
                                                       outChannelNb);
    //调用AudioTrack.play方法
    jclass audioTrackClass = env->GetObjectClass(audioTrackInstance);
    jmethodID audioTrackPlayMid = env->GetMethodID(audioTrackClass, "play", "()V");
    env->CallVoidMethod(audioTrackInstance, audioTrackPlayMid);

    // 获取AudioTrack的write方法 public int write(@NonNull byte[] audioData, int offsetInBytes, int sizeInBytes)
    jmethodID writeMid = env->GetMethodID(audioTrackClass, "write", "([BII)I");

    uint8_t *outBuffer = static_cast<uint8_t *>(av_malloc(MAX_AUDIO_FRME_SIZE));

    int gotFrame = 0, index = 0, ret;
    LOG_D("音频开始解码");
    while (av_read_frame(gFormatCtx, packet) >= 0) {
        if (packet->stream_index == gAudioIndex) {
            // 解码
            ret = avcodec_send_packet(pCodecCtx, packet);
            gotFrame = avcodec_receive_frame(pCodecCtx, frame);

            if (ret != 0) {
                LOG_D("解码出错");
                return NULL;
            }
            if (gotFrame == 0) {
                LOG_D("音频解码：第%d帧", index++);
                swr_convert(pSwrCtx, &outBuffer, MAX_AUDIO_FRME_SIZE,
                            (const uint8_t **) (frame->data), frame->nb_samples);


                int outBufferSize = av_samples_get_buffer_size(NULL, outChannelNb,
                                                               frame->nb_samples, outSampleFmt, 1);

                jbyteArray audioSampleArray = env->NewByteArray(outBufferSize);
                jbyte *sampleByte = env->GetByteArrayElements(audioSampleArray, NULL);
                memcpy(sampleByte, outBuffer, outBufferSize);

                // 非常重要的一步  没这步 无数据  同步
                env->ReleaseByteArrayElements(audioSampleArray, sampleByte, 0);

                env->CallIntMethod(audioTrackInstance, writeMid, audioSampleArray, 0,
                                   outBufferSize);

                usleep(1000 * 16);

                env->DeleteLocalRef(audioSampleArray);
            }

        }
    }

    av_frame_free(&frame);
    av_free(outBuffer);
    swr_free(&pSwrCtx);

    env->DeleteGlobalRef(gInstance);
    env->DeleteGlobalRef(gFFmpegClass);
    gJvm->DetachCurrentThread();
    return NULL;
}


int render::Sound(JNIEnv *env, jobject jthiz, AVFormatContext *pFormatCtx, int audioIndex) {
    gAudioIndex = audioIndex;
    gFormatCtx = pFormatCtx;

    env->GetJavaVM(&gJvm);;
    gInstance = env->NewGlobalRef(jthiz);
    jclass thisClass = env->GetObjectClass(jthiz);
    gFFmpegClass = static_cast<jclass>(env->NewGlobalRef(thisClass));

    pthread_t thread;
    pthread_create(&thread, NULL, RenderSoundTask, NULL);

    return 0;
}
