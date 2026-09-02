const { app, BrowserWindow, ipcMain, nativeImage } = require('electron');
const path = require('path');
const net = require('net');
const { spawn } = require('child_process');
const { WebSocketServer } = require('ws');
const isDev = !app.isPackaged;

app.setName('C4D Portal');

const SIGNALING_PORT = 8787;
const USB_PORT = 9090;

let mainWindow = null;
let wss = null;
let phoneSocket = null; // the one connected phone client, if any

let virtualCamera = null;
try {
  virtualCamera = require('c4dportal-virtual-camera');
} catch (err) {
  console.error('c4dportal-virtual-camera addon failed to load — run `npm run rebuild:native`.', err);
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1536,
    height: 1024,
    minWidth: 1100,
    minHeight: 700,
    backgroundColor: '#0a0e14',
    show: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.cjs'),
    },
    title: 'C4D Portal',
  });

  mainWindow.once('ready-to-show', () => mainWindow.show());

  if (isDev) {
    mainWindow.loadURL('http://localhost:5173');
    mainWindow.webContents.openDevTools();
    mainWindow.webContents.on('did-fail-load', () => {
      setTimeout(() => mainWindow.loadURL('http://localhost:5173'), 500);
    });
  } else {
    mainWindow.loadFile(path.join(__dirname, 'dist', 'index.html'));
  }
}

// ── WiFi signaling ──────────────────────────────────────────────────────
// Minimal WebSocket relay: the phone connects to ws://<host>:8787/signal
// and exchanges WebRTC offer/answer/ICE messages. The actual RTCPeerConnection
// lives in the renderer (only Chromium has WebRTC APIs, not Node), so this
// just forwards JSON messages between the phone socket and the renderer via
// IPC. See docs/protocol.md "WiFi transport".
function startSignalingServer() {
  if (wss) return;
  wss = new WebSocketServer({ port: SIGNALING_PORT, path: '/signal' });

  wss.on('connection', (socket) => {
    // Single-phone MVP: a new connection replaces any previous one.
    if (phoneSocket) phoneSocket.close();
    phoneSocket = socket;
    mainWindow?.webContents.send('signal:phone-connected');

    socket.on('message', (raw) => {
      try {
        const msg = JSON.parse(raw.toString());
        mainWindow?.webContents.send('signal:from-phone', msg);
      } catch (err) {
        console.error('signal: bad message from phone', err);
      }
    });

    socket.on('close', () => {
      if (phoneSocket === socket) {
        phoneSocket = null;
        mainWindow?.webContents.send('signal:phone-disconnected');
      }
    });
  });
}

function stopSignalingServer() {
  phoneSocket?.close();
  phoneSocket = null;
  wss?.close();
  wss = null;
}

ipcMain.handle('signal:start', () => {
  startSignalingServer();
  return true;
});

ipcMain.handle('signal:stop', () => {
  stopSignalingServer();
  return true;
});

ipcMain.handle('signal:send-to-phone', (_event, msg) => {
  if (!phoneSocket) return false;
  phoneSocket.send(JSON.stringify(msg));
  return true;
});

// ── USB transport ────────────────────────────────────────────────────────
// No WebRTC/ICE needed here — `adb reverse` gives a direct TCP tunnel, so
// the phone just dials 127.0.0.1:<USB_PORT> on-device and that's forwarded
// straight to this listening server. Wire format: [4-byte BE length][JPEG
// bytes], repeated. Each JPEG is decoded with Electron's built-in image
// decoder (nativeImage) — no extra native dependency needed — then handed
// to the same virtual-camera bridge the WiFi path uses. See
// docs/protocol.md "USB transport".
let usbServer = null;
let usbSocket = null;
let adbReverseProcess = null;

function startUsbBridge() {
  if (usbServer) return;

  adbReverseProcess = spawn('adb', ['reverse', `tcp:${USB_PORT}`, `tcp:${USB_PORT}`]);
  adbReverseProcess.on('error', (err) => {
    console.error('adb reverse failed to start — is adb on PATH?', err);
    mainWindow?.webContents.send('usb:error', 'adb not found on PATH');
  });

  usbServer = net.createServer((socket) => {
    console.log('usb: phone connected from', socket.remoteAddress, socket.remotePort);
    if (usbSocket) usbSocket.destroy();
    usbSocket = socket;
    mainWindow?.webContents.send('usb:phone-connected');

    let lengthBuf = Buffer.alloc(0);
    let expectedLength = null;
    let frameChunks = [];
    let frameBytesRead = 0;
    let sawFirstChunk = false;

    socket.on('data', (chunk) => {
      if (!sawFirstChunk) {
        sawFirstChunk = true;
        console.log('usb: first data chunk received,', chunk.length, 'bytes');
      }
      let offset = 0;
      while (offset < chunk.length) {
        if (expectedLength === null) {
          const needed = 4 - lengthBuf.length;
          const take = Math.min(needed, chunk.length - offset);
          lengthBuf = Buffer.concat([lengthBuf, chunk.subarray(offset, offset + take)]);
          offset += take;
          if (lengthBuf.length === 4) {
            expectedLength = lengthBuf.readUInt32BE(0);
            lengthBuf = Buffer.alloc(0);
            frameChunks = [];
            frameBytesRead = 0;
          }
        } else {
          const remaining = expectedLength - frameBytesRead;
          const take = Math.min(remaining, chunk.length - offset);
          frameChunks.push(chunk.subarray(offset, offset + take));
          frameBytesRead += take;
          offset += take;
          if (frameBytesRead === expectedLength) {
            handleUsbJpegFrame(Buffer.concat(frameChunks));
            expectedLength = null;
          }
        }
      }
    });

    socket.on('close', () => {
      if (usbSocket === socket) {
        usbSocket = null;
        mainWindow?.webContents.send('usb:phone-disconnected');
      }
    });
    socket.on('error', (err) => console.error('usb: socket error', err));
  });

  usbServer.on('error', (err) => console.error('usb: server error', err));
  usbServer.listen(USB_PORT, '127.0.0.1', () => console.log('usb: listening on 127.0.0.1:' + USB_PORT));
}

// Applied to what's actually pushed to the virtual camera (not just the
// on-screen preview, which the renderer flips separately via canvas/CSS
// transform — see PreviewPanel.tsx) so OBS/etc. see the flipped image too.
// Set via the "Camera Flip (Horizontal/Vertical)" toggles in Settings.
let flipHorizontal = false;
let flipVertical = false;

// Was doing one Buffer.copy() call PER PIXEL for horizontal flip — for a
// 1280x720 frame that's ~920,000 tiny function calls every frame, which is
// exactly what caused "extreme lag" whenever a flip toggle was on. Uint32
// typed-array views (one BGRA pixel = one uint32) turn per-pixel swaps into
// plain numeric array indexing, and the no-horizontal-flip row case uses a
// single bulk TypedArray#set instead of a byte-by-byte copy — both are
// close to native speed in V8.
// Uint32Array requires its byteOffset into the backing buffer to be a
// multiple of 4, which a Buffer's offset isn't guaranteed to be (pooled
// Buffers in particular). Copy into a fresh, always-0-offset Uint8Array
// when it isn't already aligned rather than risk a RangeError.
function toAlignedUint32View(buf) {
  if (buf.byteOffset % 4 === 0) {
    return new Uint32Array(buf.buffer, buf.byteOffset, buf.length / 4);
  }
  const aligned = new Uint8Array(buf.length);
  aligned.set(buf);
  return new Uint32Array(aligned.buffer);
}

function flipBgraInPlace(bgra, width, height, flipH, flipV) {
  if (!flipH && !flipV) return bgra;

  const pixelCount = width * height;
  const srcView = toAlignedUint32View(bgra);
  const out = Buffer.alloc(bgra.length);
  const dstView = toAlignedUint32View(out);

  for (let y = 0; y < height; y++) {
    const srcY = flipV ? height - 1 - y : y;
    const srcRowStart = srcY * width;
    const dstRowStart = y * width;
    if (flipH) {
      for (let x = 0; x < width; x++) {
        dstView[dstRowStart + x] = srcView[srcRowStart + (width - 1 - x)];
      }
    } else {
      dstView.set(srcView.subarray(srcRowStart, srcRowStart + width), dstRowStart);
    }
  }
  return out;
}

// Same 0-100 slider -> 0-200% mapping as desktop/src/lib/imageAdjustments.ts
// (50 -> 100% / neutral) so USB and WiFi look identical for the same
// slider position. 100/100/100 (i.e. sliders at 50/50/50) is the neutral
// case and skips the per-pixel pass entirely.
let adjustBrightnessPct = 100;
let adjustContrastPct = 100;
let adjustSaturationPct = 100;

function clampByte(v) {
  return v < 0 ? 0 : v > 255 ? 255 : v;
}

function adjustBgraInPlace(bgra, brightnessPct, contrastPct, saturationPct) {
  if (brightnessPct === 100 && contrastPct === 100 && saturationPct === 100) return bgra;

  const brightnessFactor = brightnessPct / 100;
  const contrastFactor = contrastPct / 100;
  const saturationFactor = saturationPct / 100;
  const out = Buffer.alloc(bgra.length);

  for (let i = 0; i < bgra.length; i += 4) {
    let b = bgra[i];
    let g = bgra[i + 1];
    let r = bgra[i + 2];

    // Contrast around the mid-gray point, then brightness as a straight
    // multiplier — same order most simple image editors use.
    r = (r - 128) * contrastFactor + 128;
    g = (g - 128) * contrastFactor + 128;
    b = (b - 128) * contrastFactor + 128;
    r *= brightnessFactor;
    g *= brightnessFactor;
    b *= brightnessFactor;

    // Saturation: blend each channel toward (or away from) perceptual
    // luma — the standard "lerp with grayscale" approach.
    const luma = 0.299 * r + 0.587 * g + 0.114 * b;
    r = luma + (r - luma) * saturationFactor;
    g = luma + (g - luma) * saturationFactor;
    b = luma + (b - luma) * saturationFactor;

    out[i] = clampByte(b);
    out[i + 1] = clampByte(g);
    out[i + 2] = clampByte(r);
    out[i + 3] = bgra[i + 3];
  }
  return out;
}

let usbFrameCount = 0;
function handleUsbJpegFrame(jpegBuffer) {
  try {
    const image = nativeImage.createFromBuffer(jpegBuffer);
    const { width, height } = image.getSize();
    if (width === 0 || height === 0) {
      console.warn('usb: decoded frame has zero size, jpeg byte length was', jpegBuffer.length);
      return;
    }
    let bgra = adjustBgraInPlace(image.toBitmap(), adjustBrightnessPct, adjustContrastPct, adjustSaturationPct);
    bgra = flipBgraInPlace(bgra, width, height, flipHorizontal, flipVertical);
    virtualCamera?.pushFrame(bgra, width, height);
    mainWindow?.webContents.send('usb:frame', jpegBuffer, width, height);
    usbFrameCount += 1;
    if (usbFrameCount % 30 === 1) console.log('usb: frame', usbFrameCount, width, 'x', height);
  } catch (err) {
    console.error('usb: failed to decode frame', err);
  }
}

function stopUsbBridge() {
  usbSocket?.destroy();
  usbSocket = null;
  usbServer?.close();
  usbServer = null;
  adbReverseProcess?.kill();
  adbReverseProcess = null;
  spawn('adb', ['reverse', '--remove', `tcp:${USB_PORT}`]).on('error', () => {});
}

ipcMain.handle('usb:start', () => {
  startUsbBridge();
  return true;
});

ipcMain.handle('usb:stop', () => {
  stopUsbBridge();
  return true;
});

// Control channel reuses the same TCP connection as the frame stream —
// TCP is full-duplex, so we can write commands to the phone on the same
// socket it's streaming frames from. "SWITCH_CAMERA:<facing>\n" is
// unambiguous next to the binary length-prefixed frame data because the
// phone side only reads commands between frame sends, on its own dedicated
// read loop — see UsbTransport.kt's `connect()`. The facing is explicit
// (not a toggle) so it can't drift out of sync with the phone's actual
// state if the phone's own on-device button gets used too.
ipcMain.handle('usb:switch-camera', (_event, facing) => {
  if (!usbSocket) return false;
  usbSocket.write(`SWITCH_CAMERA:${facing}\n`);
  return true;
});

// ── Virtual camera bridge ───────────────────────────────────────────────
ipcMain.handle('camera:create', (_event, name) => {
  if (!virtualCamera) return { ok: false, error: 'native addon not loaded' };
  try {
    virtualCamera.create(name || 'C4D Portal');
    return { ok: true };
  } catch (err) {
    return { ok: false, error: String(err.message || err) };
  }
});

ipcMain.handle('camera:start', () => {
  if (!virtualCamera) return { ok: false, error: 'native addon not loaded' };
  try {
    virtualCamera.start();
    return { ok: true };
  } catch (err) {
    return { ok: false, error: String(err.message || err) };
  }
});

ipcMain.handle('camera:stop', () => {
  virtualCamera?.stop();
  return true;
});

ipcMain.handle('camera:set-flip', (_event, h, v) => {
  flipHorizontal = Boolean(h);
  flipVertical = Boolean(v);
  return true;
});

// brightness/contrast/saturation arrive as raw 0-100 slider values —
// convert to the 0-200% scale adjustBgraInPlace expects (matches
// desktop/src/lib/imageAdjustments.ts's sliderToPercent).
ipcMain.handle('camera:set-adjustments', (_event, brightness, contrast, saturation) => {
  adjustBrightnessPct = Number(brightness) * 2;
  adjustContrastPct = Number(contrast) * 2;
  adjustSaturationPct = Number(saturation) * 2;
  return true;
});

ipcMain.on('camera:push-frame', (_event, buffer, width, height) => {
  try {
    virtualCamera?.pushFrame(buffer, width, height);
  } catch {
    // Best-effort — a dropped frame here isn't worth crashing the app over.
  }
});

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  stopSignalingServer();
  stopUsbBridge();
  virtualCamera?.stop();
  if (process.platform !== 'darwin') app.quit();
});
