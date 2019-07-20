package com.example.incubed

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import in3.IN3
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    val in3 by lazy { IN3() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        sbtn.setOnClickListener {
            GlobalScope.launch(Dispatchers.Main) {
                val res = withContext(Dispatchers.Default) {
                    in3.send("{\"jsonrpc\":\"2.0\",\"method\":\"eth_getBlockByNumber\",\"params\":[\"0x1b4\", true],\"id\":1}")
                }
                helloCom.text = res
            }

        }
    }
}
