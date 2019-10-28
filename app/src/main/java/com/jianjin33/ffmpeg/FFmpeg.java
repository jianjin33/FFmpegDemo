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
        System.loadLibrary("libffmpeg");
    }

    public native String init();

    public String show(String msg){
        Toast.makeText(ctx,msg+"",Toast.LENGTH_SHORT).show();
        return "javaTest";
    }
}
