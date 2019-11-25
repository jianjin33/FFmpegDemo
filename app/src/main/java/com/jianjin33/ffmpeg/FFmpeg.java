package com.jianjin33.ffmpeg;

import android.content.Context;
import android.view.Surface;
import android.widget.Toast;

public class FFmpeg {


    private Context ctx;

    public FFmpeg(Context ctx) {
        this.ctx = ctx;
    }

    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("ffmpeg");
        System.loadLibrary("yuv");
    }

    //public native int init();

    //public native int open(String path);

    public native int render(String path, Surface surface);

    public String show(String msg){
        Toast.makeText(ctx,msg+"",Toast.LENGTH_SHORT).show();
        return "javaTest";
    }
}
