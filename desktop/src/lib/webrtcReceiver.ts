import type { SignalingMessage } from '../c4dportal-bridge';

export interface WebrtcReceiverCallbacks {
  onStream: (stream: MediaStream) => void;
  onDisconnected: () => void;
}

interface CapturedFrame {
  data: Uint8Array;
  width: number;
  height: number;
}

// Runs in the renderer (only Chromium has WebRTC APIs — the Electron main
// process is plain Node and can't do this). Receives the phone's video
// track over a peer connection negotiated through the main process's
// WebSocket signaling relay (see electron-main.cjs "WiFi signaling").
//
// Two independent consumers of the incoming video:
//  - the UI binds directly to `stream` (onStream callback) for on-screen
//    preview, decoupled from anything below.
//  - captureBgraFrame() pulls a still frame on demand, for whoever's
//    driving the push-to-virtual-camera loop (App.tsx, on an interval).
export class WebrtcReceiver {
  private pc: RTCPeerConnection | null = null;
  private captureVideo: HTMLVideoElement;
  private captureCanvas: HTMLCanvasElement;
  private captureCtx: CanvasRenderingContext2D;
  private unsubscribers: Array<() => void> = [];

  constructor(private callbacks: WebrtcReceiverCallbacks) {
    this.captureVideo = document.createElement('video');
    this.captureVideo.muted = true;
    this.captureVideo.playsInline = true;
    this.captureCanvas = document.createElement('canvas');
    const ctx = this.captureCanvas.getContext('2d', { willReadFrequently: true });
    if (!ctx) throw new Error('2D canvas context unavailable');
    this.captureCtx = ctx;
  }

  async start() {
    await window.c4dportal.signaling.start();
    this.unsubscribers.push(
      window.c4dportal.signaling.onMessageFromPhone((msg) => this.handleSignal(msg)),
      window.c4dportal.signaling.onPhoneDisconnected(() => this.teardownPeerConnection()),
    );
  }

  async stop() {
    this.unsubscribers.forEach((u) => u());
    this.unsubscribers = [];
    await window.c4dportal.signaling.stop();
    this.teardownPeerConnection();
  }

  private async handleSignal(msg: SignalingMessage) {
    if (msg.type === 'offer' && msg.sdp) {
      await this.handleOffer(msg.sdp);
    } else if (msg.type === 'ice' && msg.candidate) {
      try {
        await this.pc?.addIceCandidate(msg.candidate);
      } catch (err) {
        console.error('addIceCandidate failed', err);
      }
    }
  }

  private async handleOffer(sdp: string) {
    this.teardownPeerConnection();

    // No STUN/TURN servers — phone and desktop are expected to be on the
    // same LAN, so host ICE candidates are sufficient.
    const pc = new RTCPeerConnection();
    this.pc = pc;

    pc.ontrack = (event) => {
      const stream = event.streams[0] ?? new MediaStream([event.track]);
      this.captureVideo.srcObject = stream;
      this.captureVideo.play().catch(() => {});
      this.callbacks.onStream(stream);
    };

    pc.onicecandidate = (event) => {
      if (event.candidate) {
        window.c4dportal.signaling.sendToPhone({ type: 'ice', candidate: event.candidate.toJSON() });
      }
    };

    pc.onconnectionstatechange = () => {
      if (pc.connectionState === 'disconnected' || pc.connectionState === 'failed' || pc.connectionState === 'closed') {
        this.teardownPeerConnection();
        this.callbacks.onDisconnected();
      }
    };

    await pc.setRemoteDescription({ type: 'offer', sdp });
    const answer = await pc.createAnswer();
    await pc.setLocalDescription(answer);
    window.c4dportal.signaling.sendToPhone({ type: 'answer', sdp: answer.sdp });
  }

  // Draws the current video frame and returns it as BGRA bytes (matching
  // what SharedFrameBuffer / C4DPortalMediaSource.dll expect — see
  // docs/protocol.md "Frame delivery to the virtual camera"). Canvas
  // ImageData is RGBA, so this swaps the R/B channels. Returns null if no
  // frame is available yet.
  captureBgraFrame(flipHorizontal = false, flipVertical = false): CapturedFrame | null {
    const { videoWidth, videoHeight } = this.captureVideo;
    if (videoWidth === 0 || videoHeight === 0) return null;

    if (this.captureCanvas.width !== videoWidth || this.captureCanvas.height !== videoHeight) {
      this.captureCanvas.width = videoWidth;
      this.captureCanvas.height = videoHeight;
    }

    this.captureCtx.save();
    this.captureCtx.translate(flipHorizontal ? videoWidth : 0, flipVertical ? videoHeight : 0);
    this.captureCtx.scale(flipHorizontal ? -1 : 1, flipVertical ? -1 : 1);
    this.captureCtx.drawImage(this.captureVideo, 0, 0, videoWidth, videoHeight);
    this.captureCtx.restore();

    const imageData = this.captureCtx.getImageData(0, 0, videoWidth, videoHeight);
    const rgba = imageData.data;
    const bgra = new Uint8Array(rgba.length);
    for (let i = 0; i < rgba.length; i += 4) {
      bgra[i] = rgba[i + 2];
      bgra[i + 1] = rgba[i + 1];
      bgra[i + 2] = rgba[i];
      bgra[i + 3] = rgba[i + 3];
    }
    return { data: bgra, width: videoWidth, height: videoHeight };
  }

  private teardownPeerConnection() {
    this.pc?.close();
    this.pc = null;
    this.captureVideo.srcObject = null;
  }
}
