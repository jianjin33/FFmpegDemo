package com.jianjin33.ffmpeg;

public class Request {

    static {
        System.load("native-request");
    }

    public native String requestTest();
}
