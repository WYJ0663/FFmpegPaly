#include <jni.h>
#include <string>
#include "FFmpegMusic.h"
#include "FFmpegVideo.h"
#include <android/native_window_jni.h>


extern "C" {
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
//像素处理
#include "libswscale/swscale.h"

#include <unistd.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Log.h"
}
const char *inputPath;
FFmpegVideo *ffmpegVideo;
FFmpegMusic *ffmpegMusic;
pthread_t p_tid;
bool isPlay;
ANativeWindow *window = 0;
int64_t duration;
AVFormatContext *pFormatCtx;
AVPacket *packet;

JavaVM *pJavaVM;
jobject pInstance;
bool isSize = false;
double preClock;

bool isCutImage = false;

bool checkIsPlay() {
    return isPlay && NULL != ffmpegVideo && NULL != ffmpegMusic && ffmpegVideo->isPlay && ffmpegMusic->isPlay;
}

void changeSize(int width, int height) {

    if (isSize || NULL == pJavaVM || NULL == pInstance) {
        return;
    }

    JNIEnv *env;
    pJavaVM->AttachCurrentThread(&env, NULL);

    static jmethodID methodID;
    if (NULL != env && NULL == methodID) {
        jclass clazz = env->GetObjectClass(pInstance);
        methodID = env->GetMethodID(clazz, "changeSize", "(II)V");
        env->DeleteLocalRef(clazz);
    }

    if (NULL != methodID && width > 0 && height > 0) {
        env->CallVoidMethod(pInstance, methodID, width, height);
        LOGE("changeSize %d", width);
        isSize = true;
    } else {
        LOGE("methodID == null");
    }

//    pJavaVM->DetachCurrentThread();
}

void setTotalTime(int64_t duration) {
    JNIEnv *env;
    pJavaVM->AttachCurrentThread(&env, NULL);

    static jmethodID methodID;
    if (NULL != env && NULL != pInstance && NULL == methodID) {
        jclass clazz = env->GetObjectClass(pInstance);
        methodID = env->GetMethodID(clazz, "setTotalTime", "(I)V");
        env->DeleteLocalRef(clazz);
    }

    if (NULL != methodID && duration > 0) {
        LOGE("setTotalTime == null%d", (jint) (duration / 1000));
        env->CallVoidMethod(pInstance, methodID, (jint) (duration / 1000));
    } else {
        LOGE("methodID == null");
    }
}

void setCurrentTime(double duration) {
    JNIEnv *env;
    pJavaVM->AttachCurrentThread(&env, NULL);

    static jmethodID methodID = NULL;
    if (NULL != env && NULL != pInstance && NULL == methodID) {
        jclass clazz = env->GetObjectClass(pInstance);
        methodID = env->GetMethodID(clazz, "setCurrentTime", "(I)V");
        env->DeleteLocalRef(clazz);
    }

    if (NULL != methodID && duration > 0) {
        env->CallVoidMethod(pInstance, methodID, (jint) (duration * 1000));
    } else {
        LOGE("methodID == null");
    }
}

void setCurrentImage(AVFrame *frame) {
    if (!isCutImage) {
        return;
    }
    isCutImage = false;
    LOGE("setCurrentImage isCutImage=%d",isCutImage);
    JNIEnv *env;
    pJavaVM->AttachCurrentThread(&env, NULL);

    int w = ffmpegVideo->codec->width;
    int h = ffmpegVideo->codec->height;
    uint8_t *src = frame->data[0];

    int size = w * h;
    jintArray result = env->NewIntArray(size);
    env->SetIntArrayRegion(result, 0, size, reinterpret_cast<const jint *>(src));

    static jmethodID methodID = NULL;
    if (NULL != env && NULL != pInstance && NULL == methodID) {
        jclass clazz = env->GetObjectClass(pInstance);
        methodID = env->GetMethodID(clazz, "setCurrentImage", "([III)V");
        env->DeleteLocalRef(clazz);
    }
    LOGE("setCurrentImage w=%d",w);
    if (NULL != methodID) {
        LOGE("setCurrentImage CallVoidMethod");
        env->CallVoidMethod(pInstance, methodID, result, w, h);
    } else {
        LOGE("setCurrentImage == null");
    }
}

void call_video_play(AVFrame *frame) {
    if (!window) {
        return;
    }
    ANativeWindow_Buffer window_buffer;
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        return;
    }

    LOGE("绘制 宽%d,高%d", frame->width, frame->height);
    LOGE("绘制 宽%d,高%d  行字节 %d ", window_buffer.width, window_buffer.height, frame->linesize[0]);

    uint8_t *dst = (uint8_t *) window_buffer.bits;
    int dstStride = window_buffer.stride * 4;
    uint8_t *src = frame->data[0];
    int srcStride = frame->linesize[0];
    for (int i = 0; i < window_buffer.height; ++i) {
        memcpy(dst + i * dstStride, src + i * srcStride, srcStride);
    }
    ANativeWindow_unlockAndPost(window);
    setCurrentImage(frame);
}

void call_music_play(double clock) {
    LOGE("call_music_play %lf %lf", clock, preClock);
    if ((clock - preClock) > 1) {
        preClock = clock;
        setCurrentTime(clock);
    }
}

void init() {
    LOGE("开启解码线程")
    //1.注册组件
    av_register_all();
    avformat_network_init();
    //封装格式上下文
    pFormatCtx = avformat_alloc_context();

    //2.打开输入视频文件
    if (avformat_open_input(&pFormatCtx, inputPath, NULL, NULL) != 0) {
        LOGE("%s", "打开输入视频文件失败");
    }
    //3.获取视频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("%s", "获取视频信息失败");
    }

    //得到播放总时间
    if (pFormatCtx->duration != AV_NOPTS_VALUE) {
        duration = pFormatCtx->duration;//微秒
        setTotalTime(duration);
    }
}

void stop(JNIEnv *env) {//释放资源
    isSize = false;
    preClock = 0;

    if (isPlay) {
        isPlay = false;
        pthread_join(p_tid, 0);
    }
    if (ffmpegVideo) {
        if (ffmpegVideo->isPlay) {
            ffmpegVideo->stop();
        }
        delete (ffmpegVideo);
        ffmpegVideo = 0;
    }
    if (ffmpegMusic) {
        if (ffmpegMusic->isPlay) {
            ffmpegMusic->stop();
        }
        delete (ffmpegMusic);
        ffmpegMusic = 0;
    }

    //jni接口
    if (pInstance) {
        env->DeleteGlobalRef(pInstance);
    }
}

/*void swap(int *a,int *b)
{
    int t=*a;*a=*b;*b=t;
}*/
//单位秒
void seekTo(int mesc) {
    if (!checkIsPlay()) {
        return;
    }

    preClock = mesc;

    if (mesc <= 0) {
        mesc = 0;
    }
    //清空vector
    ffmpegMusic->queue.clear();
    ffmpegVideo->queue.clear();
    //跳帧
    /* if (av_seek_frame(pFormatCtx, -1,  mesc * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD) < 0) {
         LOGE("failed")
     } else {
         LOGE("success")
     }*/

    av_seek_frame(pFormatCtx, ffmpegVideo->index, (int64_t) (mesc / av_q2d(ffmpegVideo->time_base)),
                  AVSEEK_FLAG_BACKWARD);
    av_seek_frame(pFormatCtx, ffmpegMusic->index, (int64_t) (mesc / av_q2d(ffmpegMusic->time_base)),
                  AVSEEK_FLAG_BACKWARD);

}

void *begin(void *args) {

    //找到视频流和音频流
    for (int i = 0; i < pFormatCtx->nb_streams; ++i) {
        //获取解码器
        AVCodecContext *avCodecContext = pFormatCtx->streams[i]->codec;
        AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);

        //copy一个解码器，
        AVCodecContext *codecContext = avcodec_alloc_context3(avCodec);
        avcodec_copy_context(codecContext, avCodecContext);
        if (avcodec_open2(codecContext, avCodec, NULL) < 0) {
            LOGE("打开失败")
            continue;
        }
        //如果是视频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            ffmpegVideo->index = i;
            ffmpegVideo->setAvCodecContext(codecContext);
            ffmpegVideo->time_base = pFormatCtx->streams[i]->time_base;
            if (window) {
                ANativeWindow_setBuffersGeometry(window, ffmpegVideo->codec->width,
                                                 ffmpegVideo->codec->height,
                                                 WINDOW_FORMAT_RGBA_8888);
                changeSize(ffmpegVideo->codec->width, ffmpegVideo->codec->height);
            }
        }//如果是音频流
        else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            ffmpegMusic->index = i;
            ffmpegMusic->setAvCodecContext(codecContext);
            ffmpegMusic->time_base = pFormatCtx->streams[i]->time_base;
        }
    }
    //开启播放
    ffmpegVideo->setFFmepegMusic(ffmpegMusic);
    ffmpegMusic->play();
    ffmpegVideo->play();
    isPlay = true;
    //seekTo(0);
    //解码packet,并压入队列中
    packet = (AVPacket *) av_mallocz(sizeof(AVPacket));
    //跳转到某一个特定的帧上面播放
    int ret;
    while (isPlay) {
        //
        ret = av_read_frame(pFormatCtx, packet);
        if (ret == 0) {
            if (ffmpegVideo && ffmpegVideo->isPlay && packet->stream_index == ffmpegVideo->index
                    ) {
                //将视频packet压入队列
                ffmpegVideo->put(packet);
            } else if (ffmpegMusic && ffmpegMusic->isPlay &&
                       packet->stream_index == ffmpegMusic->index) {
                ffmpegMusic->put(packet);
            }
            av_packet_unref(packet);
        } else if (ret == AVERROR_EOF) {
            // 读完了
            //读取完毕 但是不一定播放完毕
            while (isPlay) {
                if (ffmpegVideo->queue.empty() && ffmpegMusic->queue.empty()) {
                    break;
                }
                // LOGE("等待播放完成");
                av_usleep(10000);
            }
        }
    }
    //解码完过后可能还没有播放完
    isPlay = false;
    if (ffmpegMusic && ffmpegMusic->isPlay) {
        ffmpegMusic->stop();
    }
    if (ffmpegVideo && ffmpegVideo->isPlay) {
        ffmpegVideo->stop();
    }
    //释放
    av_free_packet(packet);
    avformat_free_context(pFormatCtx);
    pthread_exit(0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1play(JNIEnv *env, jobject instance, jstring inputPath_) {
    stop(env);

    env->GetJavaVM(&pJavaVM);
    pInstance = env->NewGlobalRef(instance);

    inputPath = env->GetStringUTFChars(inputPath_, 0);
    init();
    ffmpegVideo = new FFmpegVideo;
    ffmpegMusic = new FFmpegMusic;
    ffmpegVideo->setPlayCall(call_video_play);
    ffmpegMusic->setPlayCall(call_music_play);
    pthread_create(&p_tid, NULL, begin, NULL);//开启begin线程
    env->ReleaseStringUTFChars(inputPath_, inputPath);

}


extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1display(JNIEnv *env, jobject instance, jobject surface) {

    //得到界面
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    window = ANativeWindow_fromSurface(env, surface);
    if (ffmpegVideo && ffmpegVideo->codec) {
        ANativeWindow_setBuffersGeometry(window, ffmpegVideo->codec->width,
                                         ffmpegVideo->codec->height,
                                         WINDOW_FORMAT_RGBA_8888);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1stop(JNIEnv *env, jobject instance) {
    stop(env);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1pause(JNIEnv *env, jobject instance) {
    if (checkIsPlay()) {
        ffmpegMusic->pause();
        ffmpegVideo->pause();
    }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_ffmpeg_Play__1getTotalTime(JNIEnv *env, jobject instance) {

//获取视频总时间
    return (jint) duration;
}
extern "C"
JNIEXPORT double JNICALL
Java_com_ffmpeg_Play__1getCurrentPosition(JNIEnv *env, jobject instance) {
    //获取音频播放时间
    if (ffmpegMusic) {
        return ffmpegMusic->clock;
    } else {
        return 0;
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1seekTo(JNIEnv *env, jobject instance, jint msec) {
    seekTo(msec / 1000);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1stepBack(JNIEnv *env, jobject instance) {

}
extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1stepUp(JNIEnv *env, jobject instance) {
    //点击快进按钮
    seekTo(5);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1silence(JNIEnv *env, jobject instance) {

    if (checkIsPlay()) {
        if (ffmpegMusic->isSilence) {
            ffmpegMusic->isSilence = false;
        } else {
            ffmpegMusic->isSilence = true;
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1rate(JNIEnv *env, jobject instance, jfloat rate) {
    if (checkIsPlay()) {
        ffmpegMusic->rate = rate;
    }
}extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpeg_Play__1cut(JNIEnv *env, jobject instance) {

    isCutImage = true;

}