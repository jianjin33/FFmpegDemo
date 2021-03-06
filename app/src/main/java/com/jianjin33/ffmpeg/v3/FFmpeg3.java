package com.jianjin33.ffmpeg.v3;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.view.Surface;
import android.widget.Toast;

public class FFmpeg3 {


    private Context ctx;

    public FFmpeg3(Context ctx) {
        this.ctx = ctx;
    }

    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("ffmpeg");
        System.loadLibrary("yuv");
    }

    public native int render(String path, Surface surface);

    public AudioTrack createAudioTrack(int rateInHz, int nbChannels) {
        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;

        int channelConfig;
        if (nbChannels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (nbChannels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        }

        int bufferSizeInBytes = AudioTrack.getMinBufferSize(rateInHz, channelConfig, audioFormat);
        AudioTrack audioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC, rateInHz, channelConfig, audioFormat,
                bufferSizeInBytes, AudioTrack.MODE_STREAM);

        return audioTrack;

    }

}
