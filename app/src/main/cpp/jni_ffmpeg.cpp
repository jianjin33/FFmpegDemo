//
// Created by 简祖明 on 19/10/28.
//
#include <jni.h>
#include <android/log.h>
#include <string>
#include <iostream>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#define TAG "native_ffmpeg"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)

AVFormatContext *pFormatCtx;
AVCodecContext *pCodecCtx;
AVCodec *pCodec;
AVFrame *pFrame, *pFrameYUV;

uint8_t *out_buffer;
AVPacket *packet;

struct SwsContext *img_convert_ctx;

int i, videoIndex;
int frame_cnt;
int ret, got_picture;


int init_component() {
    avformat_network_init(); // 网络相关
    pFormatCtx = avformat_alloc_context();    // 注意：程序结束调用 avformat_close_input
    if (pFormatCtx) {
        return 0;
    }

    return -1;
}

void *decode_thread(void *arg) {
//    int count = *(int *) arg;
    frame_cnt = 0;
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoIndex) {
            ret = avcodec_send_packet(pCodecCtx, packet);
            got_picture = avcodec_receive_frame(pCodecCtx, pFrame);
            //ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret != 0) {
                LOG_D("解码失败！");
                return NULL;
            }

            if (got_picture == 0) {
                sws_scale(img_convert_ctx, (const uint8_t *const *) pFrame->data, pFrame->linesize,
                          0, pCodecCtx->height,
                          pFrameYUV->data, pFrameYUV->linesize);
                LOG_D("Android解码第%d帧", frame_cnt);
                frame_cnt++;
            }
            LOG_D("运行！");
        }
        av_packet_unref(packet);
    }

    avcodec_free_context(&pCodecCtx);

    //pthread_exit(&p_thread);
    return (void *) 0;
}


extern "C" JNIEXPORT jint JNICALL
Java_com_jianjin33_ffmpeg_FFmpeg_init(JNIEnv *env, jobject /* this */) {
    LOG_D("native init");
    return init_component();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_jianjin33_ffmpeg_FFmpeg_open(JNIEnv *env, jobject instance, jstring path_) {
    LOG_D("native open");
    const char *input = env->GetStringUTFChars(path_, 0);

    if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0) {
        LOG_D("获取视频文件失败！");
        return -1;
    }

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOG_D("获取视频文件信息失败！");
        return -1;
    }
    videoIndex = -1;
    // nb_streams ：输入视频的AVStream个数
    // streams ：输入视频的AVStream []数组
    for (i = 0; i != pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }

    if (videoIndex == -1) {
        LOG_D("文件中没有视频流");
        return -1;
    }

    AVCodecParameters *codecParameters = pFormatCtx->streams[videoIndex]->codecpar;

    pCodecCtx = avcodec_alloc_context3(NULL);
    if (avcodec_parameters_to_context(pCodecCtx, codecParameters) < 0) {
        LOG_D("通过AVCodecParameters生成AvCodecContext失败！");
        return -1;
    }
    // pCodecCtx = pFormatCtx->streams[videoIndex]->codec; attribute_deprecated

    if (pCodecCtx == NULL) {
        LOG_D("AVCodecContext初始化失败");
        return -1;
    }

    // 查找解码器
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        LOG_D("找不到相关解析器");
        return -1;
    }

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG_D("解析器打开失败");
        return -1;
    }

    // 分配frame内存空间，注意：程序结束调用 av_frame_free
    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();

    // 重点说明一个参数align:此参数是设定内存对齐的对齐数，也就是按多大的字节进行内存对齐。比如设置为1，
    // 表示按1字节对齐，那么得到的结果就是与实际的内存大小一样。再比如设置为4，表示按4字节对齐。也就是内存的起始地址必须是4的整倍数。
    out_buffer = (uint8_t *) av_malloc(
            av_image_get_buffer_size(AV_PIX_FMT_YUVA420P, pCodecCtx->width, pCodecCtx->height, 1));
    // 函数自身不具备内存申请的功能，此函数类似于格式化已经申请的内存，即通过av_malloc()函数申请的内存空间。
    /*  dst_data[4]：        [out]对申请的内存格式化为三个通道后，分别保存其地址
       dst_linesize[4]:        [out]格式化的内存的步长（即内存对齐后的宽度)
       src:        [in]av_alloc()函数申请的内存地址。
        pix_fmt:    [in] 申请 src内存时的像素格式
        width:        [in]申请src内存时指定的宽度
        height:        [in]申请scr内存时指定的高度
        align:        [in]申请src内存时指定的对齐字节数。*/
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer, AV_PIX_FMT_YUVA420P,
                         pCodecCtx->width,
                         pCodecCtx->height, 1);
//    avpicture_fill((AVPicture *) pFrameYUV, out_buffer, AV_PIX_FMT_YUVA420P, pCodecCtx->width, pCodecCtx->height);  attribute_deprecated

    packet = (AVPacket *) av_malloc(sizeof(AVPacket));

    av_dump_format(pFormatCtx, 0, input, 0);

    // 转换
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUVA420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);

    // FILE* fp = fopen("info.txt", "wb+");
    //fprintf(fp, "时长：%s", pFormatCtx->duration);
    //fprintf(fp, "封装格式：%s", pFormatCtx->iformat->long_name);


    pthread_t p_thread;
    pthread_create(&p_thread, NULL, decode_thread, NULL);


    env->ReleaseStringUTFChars(path_, input);
    return 0;
}