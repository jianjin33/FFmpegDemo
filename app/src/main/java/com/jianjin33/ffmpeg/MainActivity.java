package com.jianjin33.ffmpeg;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {

    private Request request;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        request = new Request(this);
    }

    public void click(View view) {
        String result = request.requestTest();
        Log.d("MainActivity", result);
    }

    public void show(int msg){
        Toast.makeText(this,msg+"",Toast.LENGTH_SHORT).show();
    }
}
