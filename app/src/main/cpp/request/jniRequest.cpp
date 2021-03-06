/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <cstring>
#include <android/log.h>
#include <string>
#include <reader.h>
#include "JniUtils.h"
#include "web_task.h"
/* Header for class com_jianjin33_ffmpeg_Request */

#ifndef _Included_com_jianjin33_ffmpeg_Request
#define _Included_com_jianjin33_ffmpeg_Request


#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_jianjin33_ffmpeg_Request
 * Method:    requestTest
 * Signature: ()Ljava/lang/String;
 */
#define TAG "Native_request"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)

using namespace std;


/**
 * 调用java方法示例
 * @param str
 */
void show(JNIEnv *env, jobject thiz, string msg) {
    // 获取该对象类
    jclass clazz = env->GetObjectClass(thiz);
    jobject m_object = (*env).NewGlobalRef(thiz);//创建对象的本地变量
    // 获取java方法ID
    jmethodID m_mid = env->GetMethodID(clazz, "show", "(Ljava/lang/String;)Ljava/lang/String;");

//    jfieldID m_fid = env->GetFieldID(clazz, "a", "I");
//    jint i = 2;
//    env->SetIntField(m_object, m_fid, i);

    jstring jmsg = env->NewStringUTF(msg.c_str());
    jstring jcallback = static_cast<jstring>(env->CallObjectMethod(m_object, m_mid, jmsg));
    string callback = Jstring2string(env, jcallback);
    LOG_D("方法返回数据是：%s\n", callback.c_str());
}

JNIEXPORT jstring JNICALL Java_com_jianjin33_ffmpeg_Request_requestTest
        (JNIEnv *env, jobject thiz) {
    string msg = "native";
    show(env, thiz, msg);
    //GET请求
    string url = "http://www.weather.com.cn/data/sk/101280601.html";
    WebTask task;
    task.SetUrl(url.c_str());
    task.SetConnectTimeout(5);
    task.DoGetString();
    if (task.WaitTaskDone() == 0) {
        //请求服务器成功
        string jsonResult = task.GetResultString();
        LOG_D("返回的json数据是：%s\n", jsonResult.c_str());
        Json::Reader reader;
        Json::Value root;

//从字符串中读取数据
        if (reader.parse(jsonResult, root)) {
            /*//根节点
            Json::Value weatherinfo = root["weatherinfo"];
            string js1 = weatherinfo["city"].asString();
            LOGI("根节点解析: %s\n", js1.c_str());*/
            //读取子节点信息
            string city = root["weatherinfo"]["city"].asString();
            string temp = root["weatherinfo"]["temp"].asString();
            string WD = root["weatherinfo"]["WD"].asString();
            string WS = root["weatherinfo"]["WS"].asString();
            string time = root["weatherinfo"]["time"].asString();
            string result =
                    "城市：" + city + "\n温度：" + temp + "\n风向：" + WD + "\n风力：" + WS + "\n时间：" + time;
            return Str2Jstring(env, result.c_str());
        }
    } else {
        LOG_D("网络连接失败\n");
        return env->NewStringUTF("网络连接失败！");
    }
}



/*static int32_t onInputEvent(struct android_app *app) {
    int sockClient = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in sockAddr = {0};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr("https://www.pecoo.com");
    sockAddr.sin_port = htonl(80);
    int nRet = connect(sockClient, reinterpret_cast<const sockaddr *>(&sockAddr),
                       sizeof(sockaddr_in));

    char szMsg[1024] = "haha";
    nRet = send(sockClient, szMsg, strlen(szMsg), 0);

}*/


#ifdef __cplusplus
}
#endif
#endif
