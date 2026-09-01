package com.c4dportal.app.stream

import android.content.Context
import android.util.Log
import org.java_websocket.client.WebSocketClient
import org.java_websocket.handshake.ServerHandshake
import org.json.JSONObject
import org.webrtc.Camera2Capturer
import org.webrtc.Camera2Enumerator
import org.webrtc.DefaultVideoDecoderFactory
import org.webrtc.DefaultVideoEncoderFactory
import org.webrtc.EglBase
import org.webrtc.IceCandidate
import org.webrtc.MediaConstraints
import org.webrtc.PeerConnection
import org.webrtc.PeerConnectionFactory
import org.webrtc.SdpObserver
import org.webrtc.SessionDescription
import org.webrtc.SurfaceTextureHelper
import org.webrtc.SurfaceViewRenderer
import org.webrtc.VideoTrack
import java.net.URI

private const val TAG = "C4DPortalWebRtc"

/**
 * Real WiFi transport: captures the phone's camera via WebRTC's own
 * Camera2Capturer (independent of the CameraX preview shown before
 * pairing — only one process can hold a Camera2 session on a given camera
 * at a time, so MainActivity releases CameraX before calling [connect]),
 * and streams it to the desktop app over a WebRTC peer connection
 * negotiated through the desktop's WebSocket signaling relay.
 *
 * Wire format matches desktop/src/lib/webrtcReceiver.ts and
 * desktop/public/fake-phone.html exactly — see docs/protocol.md.
 */
class WifiTransport(
    private val context: Context,
    private val eglBase: EglBase,
    private val localRenderer: SurfaceViewRenderer,
    private val host: String,
    private val port: Int,
    private val lensFacing: CameraLensFacing,
) : Transport {

    enum class CameraLensFacing { FRONT, BACK }

    private var factory: PeerConnectionFactory? = null
    private var peerConnection: PeerConnection? = null
    private var capturer: Camera2Capturer? = null
    private var surfaceTextureHelper: SurfaceTextureHelper? = null
    private var videoTrack: VideoTrack? = null
    private var wsClient: WebSocketClient? = null

    companion object {
        @Volatile private var factoryInitialized = false

        private fun ensureGlobalInit(context: Context) {
            if (factoryInitialized) return
            synchronized(this) {
                if (factoryInitialized) return
                PeerConnectionFactory.initialize(
                    PeerConnectionFactory.InitializationOptions.builder(context.applicationContext)
                        .createInitializationOptions(),
                )
                factoryInitialized = true
            }
        }
    }

    override fun connect(onConnected: () -> Unit, onDisconnected: (reason: String) -> Unit) {
        ensureGlobalInit(context)

        val encoderFactory = DefaultVideoEncoderFactory(eglBase.eglBaseContext, true, true)
        val decoderFactory = DefaultVideoDecoderFactory(eglBase.eglBaseContext)
        factory = PeerConnectionFactory.builder()
            .setVideoEncoderFactory(encoderFactory)
            .setVideoDecoderFactory(decoderFactory)
            .createPeerConnectionFactory()

        val cameraEnumerator = Camera2Enumerator(context)
        val cameraId = cameraEnumerator.deviceNames.firstOrNull {
            if (lensFacing == CameraLensFacing.FRONT) cameraEnumerator.isFrontFacing(it)
            else cameraEnumerator.isBackFacing(it)
        } ?: cameraEnumerator.deviceNames.firstOrNull()

        if (cameraId == null) {
            onDisconnected("No camera available")
            return
        }

        val cam2Capturer = Camera2Capturer(context, cameraId, null)
        capturer = cam2Capturer

        val helper = SurfaceTextureHelper.create("C4DPortalCaptureThread", eglBase.eglBaseContext)
        surfaceTextureHelper = helper

        val videoSource = factory!!.createVideoSource(cam2Capturer.isScreencast)
        cam2Capturer.initialize(helper, context, videoSource.capturerObserver)
        cam2Capturer.startCapture(1280, 720, 30)

        val track = factory!!.createVideoTrack("C4DPORTAL_VIDEO0", videoSource)
        track.addSink(localRenderer)
        videoTrack = track

        val rtcConfig = PeerConnection.RTCConfiguration(emptyList())
        val pc = factory!!.createPeerConnection(rtcConfig, object : PeerConnection.Observer {
            override fun onIceCandidate(candidate: IceCandidate) {
                sendToDesktop(JSONObject().apply {
                    put("type", "ice")
                    put("candidate", JSONObject().apply {
                        put("candidate", candidate.sdp)
                        put("sdpMid", candidate.sdpMid)
                        put("sdpMLineIndex", candidate.sdpMLineIndex)
                    })
                })
            }

            override fun onConnectionChange(newState: PeerConnection.PeerConnectionState?) {
                Log.i(TAG, "connection state: $newState")
                when (newState) {
                    PeerConnection.PeerConnectionState.CONNECTED -> onConnected()
                    PeerConnection.PeerConnectionState.FAILED,
                    PeerConnection.PeerConnectionState.CLOSED,
                    PeerConnection.PeerConnectionState.DISCONNECTED -> onDisconnected("peer connection $newState")
                    else -> {}
                }
            }

            override fun onIceConnectionChange(p0: PeerConnection.IceConnectionState?) {}
            override fun onIceConnectionReceivingChange(p0: Boolean) {}
            override fun onIceGatheringChange(p0: PeerConnection.IceGatheringState?) {}
            override fun onIceCandidatesRemoved(p0: Array<out IceCandidate>?) {}
            override fun onAddStream(p0: org.webrtc.MediaStream?) {}
            override fun onRemoveStream(p0: org.webrtc.MediaStream?) {}
            override fun onDataChannel(p0: org.webrtc.DataChannel?) {}
            override fun onRenegotiationNeeded() {}
            override fun onSignalingChange(p0: PeerConnection.SignalingState?) {}
            override fun onTrack(transceiver: org.webrtc.RtpTransceiver?) {}
        }) ?: run {
            onDisconnected("createPeerConnection returned null")
            return
        }
        peerConnection = pc
        pc.addTrack(track, listOf("C4DPORTAL_STREAM"))

        val client = object : WebSocketClient(URI("ws://$host:$port/signal")) {
            override fun onOpen(handshakedata: ServerHandshake?) {
                Log.i(TAG, "signaling connected — creating offer")
                val constraints = MediaConstraints()
                pc.createOffer(object : SdpObserver by NoOpSdpObserver {
                    override fun onCreateSuccess(desc: SessionDescription) {
                        pc.setLocalDescription(object : SdpObserver by NoOpSdpObserver {
                            override fun onSetSuccess() {
                                sendToDesktop(JSONObject().apply {
                                    put("type", "offer")
                                    put("sdp", desc.description)
                                })
                            }
                        }, desc)
                    }
                }, constraints)
            }

            override fun onMessage(message: String?) {
                if (message == null) return
                val msg = JSONObject(message)
                when (msg.optString("type")) {
                    "answer" -> pc.setRemoteDescription(
                        NoOpSdpObserver,
                        SessionDescription(SessionDescription.Type.ANSWER, msg.getString("sdp")),
                    )
                    "ice" -> {
                        val cand = msg.getJSONObject("candidate")
                        pc.addIceCandidate(
                            IceCandidate(
                                cand.optString("sdpMid"),
                                cand.optInt("sdpMLineIndex"),
                                cand.getString("candidate"),
                            ),
                        )
                    }
                }
            }

            override fun onClose(code: Int, reason: String?, remote: Boolean) {
                onDisconnected(reason ?: "signaling socket closed")
            }

            override fun onError(ex: Exception?) {
                Log.e(TAG, "signaling error", ex)
                onDisconnected(ex?.message ?: "signaling error")
            }
        }
        wsClient = client
        client.connect()
    }

    private fun sendToDesktop(json: JSONObject) {
        wsClient?.send(json.toString())
    }

    override fun sendEncodedFrame(data: ByteArray, presentationTimeUs: Long, isKeyFrame: Boolean) {
        // Not used for the WebRTC path — WebRTC pulls frames straight from
        // the capturer/encoder pipeline above rather than through this
        // interface method (that's what the USB path uses instead).
    }

    override fun disconnect() {
        wsClient?.close()
        wsClient = null
        peerConnection?.close()
        peerConnection = null
        capturer?.stopCapture()
        capturer?.dispose()
        capturer = null
        videoTrack?.dispose()
        videoTrack = null
        surfaceTextureHelper?.dispose()
        surfaceTextureHelper = null
        factory?.dispose()
        factory = null
    }
}

private object NoOpSdpObserver : SdpObserver {
    override fun onCreateSuccess(p0: SessionDescription?) {}
    override fun onSetSuccess() {}
    override fun onCreateFailure(p0: String?) {}
    override fun onSetFailure(p0: String?) {}
}
