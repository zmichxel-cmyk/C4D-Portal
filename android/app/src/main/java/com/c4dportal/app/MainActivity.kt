package com.c4dportal.app

import android.Manifest
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.core.CameraSelector
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import com.c4dportal.app.databinding.ActivityMainBinding
import com.c4dportal.app.stream.Transport
import com.c4dportal.app.stream.UsbTransport
import com.c4dportal.app.stream.WifiTransport
import org.webrtc.EglBase

private const val USB_PORT = 9090 // must match electron-main.cjs USB_PORT

/**
 * Camera preview + WiFi pairing entry point. Before pairing, shows a
 * CameraX preview so the user can frame the shot and pick a lens. On
 * "Connect", CameraX releases the camera (only one process can hold a
 * Camera2 session on a given camera at a time) and [WifiTransport] takes
 * over via its own WebRTC Camera2Capturer, rendering the streamed video
 * into `webrtcRenderer` as the local monitor.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var cameraProvider: ProcessCameraProvider? = null
    private var transport: Transport? = null
    private val eglBase: EglBase by lazy { EglBase.create() }

    // Which physical camera C4D Portal streams from — surfaced as a toggle so
    // the desktop app's user can pick either lens, matching the settings
    // panel's expectation of a front/rear option.
    private var lensFacing = CameraSelector.LENS_FACING_BACK
    private var isStreaming = false

    // The desktop side's virtual camera stream is currently a fixed 16:9
    // resolution (see native/media-source), so landscape is the default —
    // portrait gets pillarboxed rather than filling the frame. Exposed as a
    // manual toggle until per-orientation stream resolution (build order
    // step 6) exists.
    private var preferPortrait = false

    private val requestCameraPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) startCameraPreview() else {
                binding.statusText.text = "Camera permission required to use C4D Portal"
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.webrtcRenderer.init(eglBase.eglBaseContext, null)
        binding.switchCameraButton.setOnClickListener { toggleLensFacing() }
        binding.orientationToggleButton.setOnClickListener { toggleOrientation() }
        binding.connectButton.setOnClickListener { onConnectWifiClicked() }
        binding.connectUsbButton.setOnClickListener { onConnectUsbClicked() }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
            == PackageManager.PERMISSION_GRANTED
        ) {
            startCameraPreview()
        } else {
            requestCameraPermission.launch(Manifest.permission.CAMERA)
        }
    }

    private fun onConnectWifiClicked() {
        if (isStreaming) {
            stopStreaming()
            return
        }

        val addressText = binding.desktopAddressInput.text?.toString()?.trim().orEmpty()
        val (host, port) = parseHostPort(addressText) ?: run {
            binding.statusText.text = "Enter the desktop app's address as host:port"
            return
        }

        // Release CameraX's hold on the camera before WebRTC's own
        // Camera2Capturer opens it.
        cameraProvider?.unbindAll()
        binding.previewView.visibility = android.view.View.GONE
        binding.webrtcRenderer.visibility = android.view.View.VISIBLE
        binding.connectButton.text = "Connecting…"
        binding.connectUsbButton.isEnabled = false
        binding.desktopAddressInput.isEnabled = false
        binding.statusText.text = "Connecting to $host:$port…"

        val facing = if (lensFacing == CameraSelector.LENS_FACING_BACK) {
            WifiTransport.CameraLensFacing.BACK
        } else {
            WifiTransport.CameraLensFacing.FRONT
        }

        val wifiTransport = WifiTransport(this, eglBase, binding.webrtcRenderer, host, port, facing)
        transport = wifiTransport
        isStreaming = true

        wifiTransport.connect(
            onConnected = {
                runOnUiThread {
                    binding.statusText.text = "Streaming to $host:$port (WiFi)"
                    binding.connectButton.text = "Disconnect"
                }
            },
            onDisconnected = { reason ->
                runOnUiThread {
                    binding.statusText.text = "Disconnected: $reason"
                    stopStreaming()
                }
            },
        )
    }

    private fun onConnectUsbClicked() {
        if (isStreaming) {
            stopStreaming()
            return
        }

        val provider = cameraProvider ?: run {
            binding.statusText.text = "Camera not ready yet"
            return
        }

        binding.connectButton.isEnabled = false
        binding.connectUsbButton.text = "Connecting…"
        binding.statusText.text = "Connecting via USB (run `adb reverse` from the desktop app first)…"

        val usbTransport = UsbTransport(
            this, this, provider, binding.previewView.surfaceProvider, lensFacing, USB_PORT,
        )
        transport = usbTransport
        isStreaming = true

        usbTransport.connect(
            onConnected = {
                runOnUiThread {
                    binding.statusText.text = "Streaming over USB"
                    binding.connectUsbButton.text = "Disconnect"
                }
            },
            onDisconnected = { reason ->
                runOnUiThread {
                    binding.statusText.text = "Disconnected: $reason"
                    stopStreaming()
                }
            },
        )
    }

    private fun stopStreaming() {
        transport?.disconnect()
        transport = null
        isStreaming = false
        binding.webrtcRenderer.visibility = android.view.View.GONE
        binding.previewView.visibility = android.view.View.VISIBLE
        binding.connectButton.text = "Connect"
        binding.connectButton.isEnabled = true
        binding.connectUsbButton.text = "USB"
        binding.connectUsbButton.isEnabled = true
        binding.desktopAddressInput.isEnabled = true
        bindPreview()
    }

    private fun parseHostPort(text: String): Pair<String, Int>? {
        val parts = text.split(":")
        if (parts.size != 2) return null
        val port = parts[1].toIntOrNull() ?: return null
        if (parts[0].isBlank()) return null
        return parts[0] to port
    }

    private fun toggleOrientation() {
        preferPortrait = !preferPortrait
        requestedOrientation = if (preferPortrait) {
            ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
        } else {
            ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }
    }

    private fun toggleLensFacing() {
        lensFacing = if (lensFacing == CameraSelector.LENS_FACING_BACK) {
            CameraSelector.LENS_FACING_FRONT
        } else {
            CameraSelector.LENS_FACING_BACK
        }
        if (!isStreaming) bindPreview()
    }

    private fun startCameraPreview() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)
        cameraProviderFuture.addListener({
            cameraProvider = cameraProviderFuture.get()
            bindPreview()
        }, ContextCompat.getMainExecutor(this))
    }

    private fun bindPreview() {
        val provider = cameraProvider ?: return

        val preview = Preview.Builder().build().also {
            it.setSurfaceProvider(binding.previewView.surfaceProvider)
        }
        val selector = CameraSelector.Builder().requireLensFacing(lensFacing).build()

        try {
            provider.unbindAll()
            provider.bindToLifecycle(this, selector, preview)
            val lensName = if (lensFacing == CameraSelector.LENS_FACING_BACK) "rear" else "front"
            binding.statusText.text = "Camera ready ($lensName) — waiting for C4D Portal desktop pairing"
        } catch (exc: Exception) {
            binding.statusText.text = "Camera init failed: ${exc.message}"
        }
    }

    override fun onDestroy() {
        transport?.disconnect()
        binding.webrtcRenderer.release()
        eglBase.release()
        super.onDestroy()
    }
}
