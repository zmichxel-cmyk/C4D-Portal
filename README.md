# C4D Portal

Turns an Android phone into a system-wide webcam for Windows — works in OBS,
Streamlabs, Zoom, Discord, Teams, and anything else with a camera picker.

## Project layout

- `desktop/` — Electron + React app (the control UI: connection, device
  info, live preview, image adjustment settings). Runs today with a
  synthetic test-pattern preview; see `npm run dev` below.
- `android/` — Native Kotlin app (CameraX capture, front/rear camera
  toggle, portrait/landscape toggle). Both transports are implemented and
  verified on real hardware: `stream/WebRtcSender.kt` (WiFi, via WebRTC)
  and `stream/UsbTransport.kt` (USB, via `adb reverse` + JPEG frames).
- `native/virtual-camera/` — C++ Node addon (loaded by the Electron main
  process) that registers "C4D Portal" as a real Windows Media Foundation
  virtual camera device (`MFCreateVirtualCamera`) and writes pushed BGRA
  frames into `SharedFrameBuffer` for the media source below to read.
- `native/media-source/` — `C4DPortalMediaSource.dll`, a classic-COM
  `IMFMediaSource` implementation that the Windows **FrameServer** service
  loads out-of-process when any app (OBS, Windows Camera, Zoom, ...) opens
  the "C4D Portal" device. Serves frames by reading the same
  `SharedFrameBuffer`. Adapted from Microsoft's
  [Windows-Camera VirtualCamera sample](https://github.com/microsoft/Windows-Camera/tree/master/Samples/VirtualCamera).
- `docs/protocol.md` — pairing + wire format design for the phone↔desktop
  link.

## Verified working end-to-end (2026-08-31)

On this dev machine — which has Android Studio's bundled JDK, a full
Android SDK, and Visual Studio Build Tools with MSVC + Windows SDK — the
hardest, highest-risk piece of the whole project is now proven:

1. `native/media-source` builds into `C4DPortalMediaSource.dll` and registers
   its CLSID under `HKLM\Software\Classes\CLSID` (requires an elevated
   `rundll32 ...,DllRegisterServer` once).
2. `native/virtual-camera`'s addon calls `MFCreateVirtualCamera` +
   `IMFVirtualCamera::Start()` — both succeed, and **"C4D Portal (Windows
   Virtual Camera)" shows up as a selectable device in OBS's Video Capture
   Device dropdown** while the addon process is running (it's a
   session-lifetime device — it disappears when the process exits).
3. Frames pushed from the addon via `pushFrame(buffer, width, height)`
   flow through `SharedFrameBuffer` (a named, cross-session shared memory
   region) into the DLL running inside the `FrameServer` service, and
   **render live in OBS's preview**.

Key gotcha, in case it resurfaces: `FrameServer` runs as
`NT AUTHORITY\LocalService` in a different Terminal Services session than
the interactive desktop app, so the shared memory object needs both a
`Global\` name (not `Local\`) *and* an explicit permissive security
descriptor (`D:(A;;GA;;;WD)`) — otherwise the DLL silently fails to read
and the camera just shows a gray placeholder.

Remaining for this piece: currently fixed at 1280x720/30fps and simple
nearest-neighbor scaling of whatever's pushed — resolution/FPS
renegotiation from the desktop settings panel is build order step 6.

**WiFi transport (step 3) is also proven end-to-end**, using
`desktop/public/fake-phone.html` (a dev-only page that acts like the future
Android sender: `getUserMedia`/synthetic canvas track → `RTCPeerConnection`
→ the Electron main process's WebSocket signaling relay at
`ws://<host>:8787/signal`). Full path confirmed working and smooth:

phone-side page → WebRTC → Electron renderer (`RTCPeerConnection`, receives
track, draws to canvas) → `pushFrame()` via IPC → native addon → shared
memory → `C4DPortalMediaSource.dll` → **live, smooth video in OBS**.

One more bug fixed along the way: `SharedFrameBuffer::Read` was only
returning a frame when its sequence number was new, so whenever the
consuming app (OBS) requested samples faster than the push side supplied
them, most requests fell back to the gray placeholder — visible as
flicker. Fixed by having it repeat the last frame instead, same as a real
camera driver would between updates.

**The real Android sender is now implemented and confirmed working live**
(`stream/WebRtcSender.kt`, using `io.getstream:stream-webrtc-android` +
`Java-WebSocket`): on "Connect," `MainActivity` releases CameraX (only one
process can hold a Camera2 session at a time), hands the camera to
WebRTC's own `Camera2Capturer`, and streams to the desktop app's address
entered in a plain host:port field. Confirmed end-to-end on real hardware:
**phone's actual rear camera → WebRTC → desktop → virtual camera → live in
OBS**, matching exactly the same signaling protocol `fake-phone.html`
validated. USB transport (`UsbTransport`) is still a stub.

## Getting the desktop app running

```bash
cd desktop
npm install
npm run dev          # Vite dev server on http://localhost:5173
npm run electron:dev # same, wrapped in the Electron shell
```

## Building the Android app

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
cd android
.\gradlew.bat assembleDebug
```
(Or just open `android/` in Android Studio.)

## Building the native pieces

```powershell
# Virtual camera addon (Node addon, loaded by Electron)
cd native/virtual-camera
npm install
npm run build   # node-gyp rebuild — needs VS Build Tools' C++ workload

# Media source DLL (loaded by the FrameServer service)
cd native/media-source
powershell -ExecutionPolicy Bypass -File build.ps1
```

After building `native/media-source`, register it once (elevated):
```powershell
rundll32.exe "C:\C4DPortal\native\media-source\build\C4DPortalMediaSource.dll",DllRegisterServer
```
If you rebuild the DLL after it's already been loaded once, run
`Restart-Service FrameServer -Force` (elevated) so the service picks up the
new binary — COM DLLs stay loaded in a process until it restarts.

## Status

Following the build order in the project plan:

- [x] Step 1: desktop UI shell built and verified in-browser; Android
      CameraX preview + front/rear toggle built and verified on a real
      device; virtual camera registers and starts for real, confirmed
      visible in OBS.
- [x] Step 2: synthetic frames pushed from the addon render live in OBS.
- [x] Step 3: WiFi transport fully working — real Android phone camera,
      captured via WebRTC's Camera2Capturer, streamed over WebRTC through
      the desktop's signaling relay, rendered live in the desktop app AND
      showing up live in OBS through the virtual camera. Confirmed on real
      hardware end-to-end, not just simulated.
- [x] Step 4: USB transport working end-to-end on real hardware. No
      WebRTC/ICE needed — `adb reverse tcp:9090 tcp:9090` (run by
      electron-main.cjs) gives a direct TCP tunnel, so the phone just
      dials its own `127.0.0.1:9090`. Frames are JPEG-compressed on-device
      (`UsbTransport.kt`, CameraX `ImageAnalysis` → `YuvImage.compressToJpeg`)
      and sent length-prefixed; the desktop decodes each with Electron's
      built-in `nativeImage` (no extra native dependency) and feeds the
      same virtual-camera bridge the WiFi path uses.
- [ ] Steps 5–8: resolution/FPS renegotiation, pairing UX (currently a
      manual host:port field), installer/auto-elevation. See the project
      plan / `docs/protocol.md`.

Bug hit and fixed while wiring USB up: the TCP connection succeeded and
CameraX's `Preview` showed live video, but zero frame bytes ever reached
the desktop. Cause: `connect()`'s background task blocked forever on
`socket.getInputStream().read()` (to detect a desktop-side disconnect)
using the *same* single-thread executor the `ImageAnalysis` analyzer was
also assigned to — so the analyzer callback could never get a turn,
silently starving frame capture. Fixed by moving the blocking
connect/read logic onto its own plain `Thread`, leaving a dedicated
executor free for the analyzer.

Also fixed post-verification: the media source was **stretching** mismatched
aspect ratios to fill the fixed 1280x720 destination instead of
letterboxing — a portrait phone capture into a 16:9 stream looked squished.
`SimpleFrameGenerator::_FillFromSharedBuffer` now scales preserving aspect
ratio with black bars, and the Android app defaults to landscape capture
(`sensorLandscape` + `configChanges` in the manifest so rotating doesn't
restart the activity mid-stream) with a manual portrait/landscape toggle
button next to the front/rear camera switch — since the desktop's stream
resolution is still fixed 16:9, portrait mode gets pillarboxed rather than
filling the frame until per-orientation resolution negotiation exists.

## Camera switch, flip, and USB latency (fixed after initial USB testing)

Three more real bugs found once USB became the primary tested path:

1. **Front/rear switch from the desktop was backwards / could drift.** The
   Settings → CAMERA dropdown originally sent a blind "toggle" command —
   correct only as long as the desktop's notion of "current camera" never
   diverged from the phone's actual state (e.g. if the phone's own on-device
   switch button got used independently, which it did during testing).
   Changed the protocol to send an explicit target (`{"facing":"front"}` /
   `SWITCH_CAMERA:front`) instead of a toggle, and `WifiTransport`/
   `UsbTransport` now resolve that to the matching camera ID directly —
   idempotent, can't drift regardless of prior state.
2. **Flip toggles caused extreme lag.** `flipBgraInPlace` in
   `electron-main.cjs` was calling `Buffer.copy()` once per pixel for a
   horizontal flip — ~920,000 tiny function calls per 1280x720 frame.
   Rewritten using `Uint32Array` views (one BGRA pixel = one uint32), which
   turns per-pixel swaps into plain typed-array indexing: ~1.4ms per flip
   at 1280x720 (tested in isolation), from something an order of magnitude
   worse.
3. **USB had real latency even with no flip.** An earlier "maximize quality"
   pass had pushed `UsbTransport`'s capture resolution up to the sensor's
   max (~3648x2736, ~10MP) — but the desktop's virtual camera output is a
   fixed 1280x720, so every pixel beyond that was JPEG-encoded on-device,
   sent over the USB tunnel, and JPEG-decoded on the desktop for nothing,
   all of it downscaled away downstream anyway. Capture now targets
   1280x720 directly (matching the actual output), with JPEG quality
   dialed back from 97 to 90 for faster encode.

"Camera Flip (Horizontal)" / "Camera Flip (Vertical)" are real now too —
two toggles under Settings → Advanced (replacing the old non-functional
"Mirror Video" toggle) that flip the actual pixels for both the on-screen
preview and what's pushed to the virtual camera, on both transports.

## Known rough edges to revisit

- Desktop pairing is a manual IP:port text field, not the QR/PIN flow from
  the mockup.
- Front-camera CameraX preview (pre-stream monitor only) renders rotated
  90° — cosmetic, not yet investigated.
- No audio track yet (video-only WebRTC stream).
- WebRTC's default adaptive bitrate is conservative even on LAN, so WiFi
  video can look softer than the phone's native quality — fixable by
  pinning a higher target bitrate / disabling adaptive resolution scaling
  (already done for the WiFi sender's own encoder; worth revisiting if it's
  still not enough).
- Portrait streaming gets pillarboxed (see above) rather than the desktop
  stream adapting its own aspect ratio.
