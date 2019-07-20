package com.example.incubed;

import android.os.AsyncTask;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import android.widget.Button;
import android.text.method.ScrollingMovementMethod;
import in3.IN3;
import java.lang.ref.WeakReference;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TextView hellocom = (TextView)this.findViewById(R.id.helloCom);
        hellocom.setMovementMethod(new ScrollingMovementMethod());
        Button btn = (Button) this.findViewById(R.id.sbtn);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                GetBlockInfo getBlockInfo = new GetBlockInfo(MainActivity.this);
                getBlockInfo.execute();
            }
        });
    }

    private static class GetBlockInfo extends AsyncTask<Void, Void, String>{

        private WeakReference<MainActivity> activityReference;

        GetBlockInfo(MainActivity context) {
            activityReference = new WeakReference<>(context);
        }

        @Override
        protected String doInBackground(Void... voids) {
            IN3 in3 = new IN3();
            String result = "";
            try {
                result = in3.send("{\"jsonrpc\":\"2.0\",\"method\":\"eth_getBlockByNumber\",\"params\":[\"0x1b4\", true],\"id\":1}");
            }catch (Exception e){
                result = e.toString();
            }

            return result;
        }

        @Override
        protected void onPostExecute(String s) {
            // get a reference to the activity if it is still there
            MainActivity activity = activityReference.get();
            if (activity == null || activity.isFinishing()) return;

            // modify the activity's UI
            TextView textView = activity.findViewById(R.id.helloCom);
            textView.setText(s);
        }
    }
}
