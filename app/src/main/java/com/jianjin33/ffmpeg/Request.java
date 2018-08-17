package com.jianjin33.ffmpeg;

public class Request {

    static {
        System.loadLibrary("native-request");
    }

    public native String requestTest();
}
