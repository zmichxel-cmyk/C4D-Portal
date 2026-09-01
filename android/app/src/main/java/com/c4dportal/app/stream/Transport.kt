package com.c4dportal.app.stream

/**
 * Common interface for the two ways C4D Portal gets frames from the phone to
 * the desktop app. WiFi uses a WebRTC peer connection over the LAN
 * (WebRtcSender.kt); USB tunnels JPEG-compressed frames over a local
 * socket exposed via `adb reverse` (UsbTransport.kt). See docs/protocol.md.
 */
interface Transport {
    fun connect(onConnected: () -> Unit, onDisconnected: (reason: String) -> Unit)
    fun sendEncodedFrame(data: ByteArray, presentationTimeUs: Long, isKeyFrame: Boolean)
    fun disconnect()
}
