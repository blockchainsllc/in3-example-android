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
        startButton.setOnClickListener {
            GlobalScope.launch(Dispatchers.Main) {
                outText.text =  withContext(Dispatchers.Default) {
                    in3.send(commandEdit.text.toString())
                }
            }
        }
    }
}