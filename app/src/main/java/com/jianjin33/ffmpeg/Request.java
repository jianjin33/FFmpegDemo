package com.jianjin33.ffmpeg;

import android.content.Context;
import android.widget.Toast;

public class Request {

    private Context ctx;

    public Request(Context ctx) {
        this.ctx = ctx;
    }

    static {
        System.loadLibrary("native-request");
    }

    public native String requestTest();

    public String show(String msg){
        Toast.makeText(ctx,msg+"",Toast.LENGTH_SHORT).show();
        return "javaTest";
    }
}
