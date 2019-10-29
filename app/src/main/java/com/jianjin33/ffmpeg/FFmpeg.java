package com.jianjin33.ffmpeg;

import android.content.Context;
import android.widget.Toast;

public class FFmpeg {


    private Context ctx;

    public FFmpeg(Context ctx) {
        this.ctx = ctx;
    }

    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("ffmpeg");
    }

    public native int init();

    public native int open(String path);

    public String show(String msg){
        Toast.makeText(ctx,msg+"",Toast.LENGTH_SHORT).show();
        return "javaTest";
    }
}
