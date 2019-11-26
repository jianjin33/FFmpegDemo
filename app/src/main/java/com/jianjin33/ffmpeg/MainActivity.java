package com.jianjin33.ffmpeg;

import android.content.Intent;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private FFmpeg fmpeg;
    private SurfaceView surfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        fmpeg = new FFmpeg(this);
        surfaceView = findViewById(R.id.video_view);
    }

    public void click(View view) {
        //int result = fmpeg.init();
        //Log.d("FFmpegMainActivity", String.valueOf(result));
    }

    public void open(View view) {

        File media = new File(Environment.getExternalStorageDirectory(), "mediaFile.mp4");
        final String path = media.getPath();
        Log.d("FFmpegMainActivity", "path:" + path);
/*        Intent intent = new Intent(Intent.ACTION_VIEW);
        Uri uri = Uri.fromFile(media);
        intent.setDataAndType(uri, "video/*");
        startActivity(intent);*/

        Surface surface = surfaceView.getHolder().getSurface();
        int result = fmpeg.render(path, surface);
        Log.d("FFmpegMainActivity", String.valueOf(result));


        new Thread(new Runnable() {
            @Override
            public void run() {



            }
        }).start();

    }

    public void show(int msg) {
        Toast.makeText(this, msg + "", Toast.LENGTH_SHORT).show();
    }
}
