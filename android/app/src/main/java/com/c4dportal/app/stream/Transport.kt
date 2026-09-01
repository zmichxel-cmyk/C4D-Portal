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

    // Switches to an explicit camera ("front" or "rear") on an already-active
    // connection. Explicit rather than a blind toggle on purpose: a toggle
    // can only stay correct if the desktop's idea of "current camera" never
    // drifts from the phone's actual state (e.g. if the phone's own
    // on-device button is used independently) — sending the target directly
    // makes each command correct regardless of prior state.
    // Default no-op so any future Transport that doesn't support it (or
    // hasn't implemented it yet) doesn't have to override it.
    fun switchCamera(facing: String) {}
}
