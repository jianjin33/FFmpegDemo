//
// Created by 简祖明 on 19/10/28.
// 解码视频文件后，通过android的native_window进行绘制操作
//
#include <jni.h>
#include <android/log.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include "android/native_window.h"
#include "android/native_window_jni.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libyuv.h"
}

#define TAG "native_ffmpeg"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)

using namespace libyuv;

namespace render{

    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrameYUV, *pFrameRGB;

    AVPacket *packet;

    int videoIndex;
    ANativeWindow *pNativeWindow;


    void *DecodeRenderTask(void *arg) {
        int ret, got_picture;
        ANativeWindow_Buffer *pNativeWindowBuffer;
        int frame_cnt = 0;
        while (av_read_frame(pFormatCtx, packet) >= 0) {
            if (packet->stream_index == videoIndex) {
                // 解码AVPacket->AVFrame
                ret = avcodec_send_packet(pCodecCtx, packet);
                got_picture = avcodec_receive_frame(pCodecCtx, pFrameYUV);
                //ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
                if (ret != 0) {
                    return NULL;
                }
                if (got_picture == 0) {
                    // 设置缓冲区属性 锁定window
                    ANativeWindow_setBuffersGeometry(pNativeWindow,
                                                     pCodecCtx->width, pCodecCtx->height,
                                                     WINDOW_FORMAT_RGBA_8888);
                    // lock后得到ANativeWindow_Buffer指针
                    ANativeWindow_lock(pNativeWindow, pNativeWindowBuffer, NULL);

                    /*  dst_data[4]：        [out]对申请的内存格式化为三个通道后，分别保存其地址
                       dst_linesize[4]:        [out]格式化的内存的步长（即内存对齐后的宽度)
                       src:        [in]av_alloc()函数申请的内存地址。和ANativeWindow_Buffer->bits是同一块内存
                        pix_fmt:    [in] 申请 src内存时的像素格式
                        width:        [in]申请src内存时指定的宽度
                        height:        [in]申请scr内存时指定的高度
                        align:        [in]申请src内存时指定的对齐字节数。*/
                    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize,
                                         static_cast<const uint8_t *>(pNativeWindowBuffer->bits),
                                         AV_PIX_FMT_ABGR,
                                         pCodecCtx->width,
                                         pCodecCtx->height, 1);

                    // 通过yuvlib库将yuv转为rgb
                    I420ToARGB(pFrameYUV->data[0], pFrameYUV->linesize[0],
                               pFrameYUV->data[2], pFrameYUV->linesize[2],
                               pFrameYUV->data[1], pFrameYUV->linesize[1],
                               pFrameRGB->data[0], pFrameRGB->linesize[0],
                               pCodecCtx->width, pCodecCtx->height);

                    // unlock后会window上绘制缓冲区rgb内容
                    ANativeWindow_unlockAndPost(pNativeWindow);
                    usleep(1000 * 16);


                    LOG_D("Android解码第%d帧", frame_cnt);
                    frame_cnt++;
                }
                LOG_D("运行！");
            }
            av_packet_unref(packet);
        }
        ANativeWindow_release(pNativeWindow);
        av_frame_free(&pFrameYUV);
        av_frame_free(&pFrameRGB);
        avcodec_free_context(&pCodecCtx);
        avformat_close_input(&pFormatCtx);
        //pthread_exit(&p_thread);
        return (void *) 0;
    }


    extern "C"
    JNIEXPORT jint JNICALL
    Java_com_jianjin33_ffmpeg_FFmpeg_render(JNIEnv *env, jobject instance, jstring path_,
                                            jobject jsurface) {

        avformat_network_init(); // 网络相关
        pFormatCtx = avformat_alloc_context();    // 注意：程序结束调用 avformat_close_input
        if (!pFormatCtx) {
            return -1;
        }

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
        for (int i = 0; i != pFormatCtx->nb_streams; i++) {
            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoIndex = i;
                break;
            }
        }

        if (videoIndex == -1) {
            LOG_D("文件中没有视频流");
            return -1;
        }


        // 获取视频解码器
        AVCodecParameters *codecParameters = pFormatCtx->streams[videoIndex]->codecpar;
        pCodecCtx = avcodec_alloc_context3(NULL);
        if (avcodec_parameters_to_context(pCodecCtx, codecParameters) < 0) {
            LOG_D("通过AVCodecParameters生成AvCodecContext失败！");
            return -1;
        }

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

        // 打开解码器
        if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
            LOG_D("解析器打开失败");
            return -1;
        }

        // 分配frame内存空间，注意：程序结束调用 av_frame_free
        pFrameYUV = av_frame_alloc();
        pFrameRGB = av_frame_alloc();

        // 初始化NativeWindow(获取NativeWindow指针的这步操作我尝试放在 AVPacket分配内存 之后会崩溃)
        pNativeWindow = ANativeWindow_fromSurface(env, jsurface);

        // 编码数据
        packet = (AVPacket *) av_malloc(sizeof(AVPacket));

        av_dump_format(pFormatCtx, 0, input, 0);


        DecodeRenderTask(NULL);
//        pthread_t p_thread;
//        pthread_create(&p_thread, NULL, DecodeRenderTask, NULL);


        env->ReleaseStringUTFChars(path_, input);
        return 0;
    }
}