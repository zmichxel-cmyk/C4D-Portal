package com.c4dportal.app.stream

import android.app.Service
import android.content.Intent
import android.os.IBinder

/**
 * Foreground service that owns the CameraX capture -> MediaCodec H.264
 * encode -> Transport pipeline once a desktop app is paired. Kept as a
 * skeleton until the encode + transport pieces (build order steps 3-5) land.
 */
class StreamingService : Service() {

    private var transport: Transport? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // TODO: read connection type + host/port from `intent`, start
        // CameraX capture -> MediaCodec encoder -> transport.sendEncodedFrame
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        transport?.disconnect()
        super.onDestroy()
    }
}
