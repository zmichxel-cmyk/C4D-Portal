package com.c4dportal.app.stream

import android.content.Context
import android.graphics.ImageFormat
import android.graphics.Rect
import android.graphics.YuvImage
import android.util.Log
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import java.io.ByteArrayOutputStream
import java.io.OutputStream
import java.net.Socket
import java.util.concurrent.Executors

private const val TAG = "C4DPortalUsb"
private const val JPEG_QUALITY = 80

/**
 * USB transport: no WebRTC/ICE needed here, since `adb reverse` (run from
 * the desktop app, see electron-main.cjs "USB transport") gives a direct
 * TCP tunnel — this just dials the phone's own localhost, which adb
 * forwards straight to the desktop's listening socket.
 *
 * Frames are JPEG-compressed (simpler and more robust than a full H.264
 * MediaCodec pipeline for a first working version — USB bandwidth is ample)
 * and sent length-prefixed: [4-byte BE length][JPEG bytes]. Binds its own
 * CameraX Preview + ImageAnalysis pair (unlike WifiTransport, USB doesn't
 * need to hand the camera to a separate WebRTC capturer — CameraX already
 * owns it).
 */
class UsbTransport(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val cameraProvider: ProcessCameraProvider,
    private val previewSurfaceProvider: Preview.SurfaceProvider,
    private val lensFacing: Int,
    private val port: Int,
) : Transport {

    private var socket: Socket? = null
    private var outputStream: OutputStream? = null

    // Two separate executors on purpose: `connectThread` blocks for the
    // lifetime of the connection (waiting to detect a desktop-side close),
    // so the camera analyzer must run elsewhere — sharing one
    // single-thread executor between them meant the analyzer callback
    // could never get a turn, silently starving frame capture entirely
    // (found via: TCP connection succeeded, but zero bytes ever arrived).
    private val analyzerExecutor = Executors.newSingleThreadExecutor()
    private var connectThread: Thread? = null
    @Volatile private var connected = false

    override fun connect(onConnected: () -> Unit, onDisconnected: (reason: String) -> Unit) {
        val thread = Thread {
            try {
                val sock = Socket("127.0.0.1", port)
                sock.tcpNoDelay = true
                socket = sock
                outputStream = sock.getOutputStream()
                connected = true

                ContextCompat.getMainExecutor(context).execute { bindCameraUseCases() }
                onConnected()

                // Blocks until the socket closes (e.g. desktop disconnects),
                // so we notice even if no local action triggered disconnect().
                sock.getInputStream().read()
                if (connected) {
                    connected = false
                    onDisconnected("connection closed by desktop")
                }
            } catch (e: Exception) {
                connected = false
                onDisconnected(e.message ?: "USB connect failed")
            }
        }
        connectThread = thread
        thread.start()
    }

    private fun bindCameraUseCases() {
        val preview = Preview.Builder().build().also { it.setSurfaceProvider(previewSurfaceProvider) }
        val analysis = ImageAnalysis.Builder()
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            .build()
        analysis.setAnalyzer(analyzerExecutor) { image -> onFrame(image) }

        val selector = CameraSelector.Builder().requireLensFacing(lensFacing).build()
        try {
            cameraProvider.unbindAll()
            cameraProvider.bindToLifecycle(lifecycleOwner, selector, preview, analysis)
        } catch (e: Exception) {
            Log.e(TAG, "bindCameraUseCases failed", e)
        }
    }

    private fun onFrame(image: ImageProxy) {
        try {
            if (connected && image.format == ImageFormat.YUV_420_888) {
                val jpeg = yuv420ToJpeg(image)
                sendEncodedFrame(jpeg, image.imageInfo.timestamp, true)
            }
        } finally {
            image.close()
        }
    }

    private fun yuv420ToJpeg(image: ImageProxy): ByteArray {
        val yBuffer = image.planes[0].buffer
        val uBuffer = image.planes[1].buffer
        val vBuffer = image.planes[2].buffer

        val ySize = yBuffer.remaining()
        val uSize = uBuffer.remaining()
        val vSize = vBuffer.remaining()

        val nv21 = ByteArray(ySize + uSize + vSize)
        yBuffer.get(nv21, 0, ySize)
        vBuffer.get(nv21, ySize, vSize)
        uBuffer.get(nv21, ySize + vSize, uSize)

        val yuvImage = YuvImage(nv21, ImageFormat.NV21, image.width, image.height, null)
        val out = ByteArrayOutputStream()
        yuvImage.compressToJpeg(Rect(0, 0, image.width, image.height), JPEG_QUALITY, out)
        return out.toByteArray()
    }

    override fun sendEncodedFrame(data: ByteArray, presentationTimeUs: Long, isKeyFrame: Boolean) {
        val stream = outputStream ?: return
        try {
            val lengthPrefix = byteArrayOf(
                (data.size ushr 24).toByte(),
                (data.size ushr 16).toByte(),
                (data.size ushr 8).toByte(),
                data.size.toByte(),
            )
            synchronized(this) {
                stream.write(lengthPrefix)
                stream.write(data)
                stream.flush()
            }
        } catch (e: Exception) {
            Log.e(TAG, "send frame failed", e)
        }
    }

    override fun disconnect() {
        connected = false
        try { socket?.close() } catch (_: Exception) {}
        socket = null
        outputStream = null
        connectThread = null
        analyzerExecutor.shutdown()
    }
}
