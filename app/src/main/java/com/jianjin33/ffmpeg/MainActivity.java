package com.jianjin33.ffmpeg;

import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Toast;

import com.jianjin33.ffmpeg.v3.FFmpeg3;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private FFmpeg ffmpeg;
    private FFmpeg3 ffmpeg3;
    private SurfaceView surfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        ffmpeg = new FFmpeg(this);
        ffmpeg3 = new FFmpeg3(this);
        surfaceView = findViewById(R.id.video_view);
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
        int result = ffmpeg.render(path, surface);
        Log.d("FFmpegMainActivity", String.valueOf(result));

    }


    public void openV3(View view) {
        File media = new File(Environment.getExternalStorageDirectory(), "mediaFile.mp4");
        final String path = media.getPath();
        Surface surface = surfaceView.getHolder().getSurface();
        int result = ffmpeg3.render(path, surface);
    }

    public void show(int msg) {
        Toast.makeText(this, msg + "", Toast.LENGTH_SHORT).show();
    }
}
