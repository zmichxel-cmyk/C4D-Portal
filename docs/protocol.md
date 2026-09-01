# C4D Portal — Pairing & Wire Protocol (draft)

Status: draft — describes the target design for build order steps 3–7. Not yet implemented (Android `Transport` implementations and the desktop receive side are stubs as of this writing).

## Pairing

1. Desktop app generates a short pairing code + QR payload containing:
   `{ "host": "<LAN IP>", "port": <signaling port>, "code": "<6-digit code>" }`.
2. Phone app scans the QR (or the user types the code manually) and opens a
   WebSocket to `ws://<host>:<port>/pair` carrying the code.
3. Desktop confirms the code, both sides exchange a session id used for the
   rest of the handshake.

## WiFi transport

- Signaling: a small WebSocket server in the Electron main process
  (`ws://<host>:<port>/signal`) used only to exchange WebRTC SDP
  offer/answer + ICE candidates.
- Media: standard WebRTC `RTCPeerConnection` with a single video track
  (H.264, hardware-encoded on the phone via `MediaCodec`). Audio track
  optional (device mic).
- Desktop receives the WebRTC video track, renders it to an offscreen
  `<video>`/canvas, and pipes decoded frames into the virtual camera addon.

## USB transport

- Requires the phone to have USB debugging enabled (same requirement as
  `scrcpy`/`adb`-based tools).
- Desktop app runs `adb reverse tcp:<port> tcp:<port>` so the phone can
  reach a local socket on the same port on the PC as if it were `localhost`.
- Phone opens a plain TCP (or WebSocket) connection to `localhost:<port>`
  and streams length-prefixed encoded frames:

  ```
  [4 bytes: frame length, big-endian][frame bytes: H.264 NAL unit(s)]
  ```

- Desktop decodes the same way as the WiFi path once frames arrive — the
  rest of the pipeline (adjustments, virtual camera push) is shared code.

## Frame delivery to the virtual camera

Regardless of transport, the desktop app normalizes decoded frames to BGRA
before calling into the native addon:

```
c4dportal_virtual_camera.pushFrame(buffer /* BGRA */, width, height)
```

## Device metadata channel

Alongside video, the phone periodically (~1/sec) sends a small JSON message
with battery percent, resolution, and Android version, consumed by the
desktop app's Device panel (`src/types.ts` → `DeviceInfo`).
