import { useCallback, useRef, useState } from 'react';
import ConnectionPanel from './components/ConnectionPanel';
import DevicePanel from './components/DevicePanel';
import ToolsPanel from './components/ToolsPanel';
import PairingPanel from './components/PairingPanel';
import PreviewPanel from './components/PreviewPanel';
import SettingsPanel from './components/SettingsPanel';
import { WebrtcReceiver } from './lib/webrtcReceiver';
import { CameraFacing, ConnectionStatus, ConnectionType, DeviceInfo, StreamSettings, StreamStats } from './types';

const DEFAULT_SETTINGS: StreamSettings = {
  cameraFacing: 'rear',
  resolution: '1920 x 1080 (FHD)',
  fps: 60,
  videoFormat: 'MJPEG',
  colorSpace: 'RGB',
  brightness: 50,
  contrast: 50,
  saturation: 60,
  sharpness: 40,
  whiteBalanceMode: 'Auto',
  lowLatencyMode: true,
  flipHorizontal: false,
  flipVertical: false,
  antiFlicker: '60Hz',
};

const MOCK_USB_DEVICE: DeviceInfo = {
  name: 'Android Device',
  osVersion: 'Android 13',
  batteryPercent: 87,
  ipAddress: '192.168.1.105',
};

// Metadata (name/battery/OS version) isn't sent by the phone app yet — see
// docs/protocol.md "Device metadata channel" — so a WiFi connection shows
// this placeholder until that's wired up.
const WIFI_DEVICE_PLACEHOLDER: DeviceInfo = {
  name: 'Android Device (WiFi)',
  osVersion: '—',
  batteryPercent: 0,
};

const PUSH_FRAME_INTERVAL_MS = 33; // ~30fps push cadence, matching the negotiated stream framerate

export default function App() {
  const [connectionType, setConnectionType] = useState<ConnectionType>('usb');
  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  const [isPaused, setIsPaused] = useState(false);
  const [settings, setSettings] = useState<StreamSettings>(DEFAULT_SETTINGS);
  const [device, setDevice] = useState<DeviceInfo | null>(null);
  const [liveStream, setLiveStream] = useState<MediaStream | null>(null);
  const [usbFrame, setUsbFrame] = useState<Uint8Array | null>(null);

  const receiverRef = useRef<WebrtcReceiver | null>(null);
  const pushIntervalRef = useRef<number | null>(null);
  const usbUnsubscribersRef = useRef<Array<() => void>>([]);
  // Read inside the push-frame interval instead of depending on `settings`
  // directly, so toggling flip mid-stream doesn't restart the interval
  // (which would introduce a stutter) — just changes what the next tick draws.
  const settingsRef = useRef(settings);
  settingsRef.current = settings;

  const stopPushLoop = useCallback(() => {
    if (pushIntervalRef.current !== null) {
      window.clearInterval(pushIntervalRef.current);
      pushIntervalRef.current = null;
    }
  }, []);

  const startPushLoop = useCallback((receiver: WebrtcReceiver) => {
    stopPushLoop();
    pushIntervalRef.current = window.setInterval(() => {
      const { flipHorizontal, flipVertical, brightness, contrast, saturation } = settingsRef.current;
      const frame = receiver.captureBgraFrame(flipHorizontal, flipVertical, brightness, contrast, saturation);
      if (frame) {
        window.c4dportal.camera.pushFrame(frame.data, frame.width, frame.height);
      }
    }, PUSH_FRAME_INTERVAL_MS);
  }, [stopPushLoop]);

  const connectUsb = useCallback(async () => {
    const camResult = await window.c4dportal.camera.create('C4D Portal');
    if (!camResult.ok) {
      console.error('virtual camera create failed:', camResult.error);
    }
    const startResult = await window.c4dportal.camera.start();
    if (!startResult.ok) {
      console.error('virtual camera start failed:', startResult.error);
    }

    usbUnsubscribersRef.current = [
      window.c4dportal.usb.onPhoneConnected(() => {
        setStatus('connected');
        setDevice(MOCK_USB_DEVICE);
      }),
      window.c4dportal.usb.onPhoneDisconnected(() => {
        setStatus('disconnected');
        setDevice(null);
        setUsbFrame(null);
      }),
      window.c4dportal.usb.onError((message) => {
        console.error('usb transport error:', message);
        setStatus('disconnected');
      }),
      window.c4dportal.usb.onFrame((jpegBuffer) => {
        setUsbFrame(jpegBuffer);
      }),
    ];
    await window.c4dportal.usb.start();
  }, []);

  const disconnectUsb = useCallback(async () => {
    usbUnsubscribersRef.current.forEach((u) => u());
    usbUnsubscribersRef.current = [];
    await window.c4dportal.usb.stop();
    await window.c4dportal.camera.stop();
    setUsbFrame(null);
  }, []);

  const connectWifi = useCallback(async () => {
    const camResult = await window.c4dportal.camera.create('C4D Portal');
    if (!camResult.ok) {
      console.error('virtual camera create failed:', camResult.error);
    }
    const startResult = await window.c4dportal.camera.start();
    if (!startResult.ok) {
      console.error('virtual camera start failed:', startResult.error);
    }

    const receiver = new WebrtcReceiver({
      onStream: (stream) => {
        setLiveStream(stream);
        setStatus('connected');
        setDevice(WIFI_DEVICE_PLACEHOLDER);
        startPushLoop(receiver);
      },
      onDisconnected: () => {
        setLiveStream(null);
        setStatus('disconnected');
        setDevice(null);
        stopPushLoop();
      },
    });
    receiverRef.current = receiver;
    await receiver.start();
  }, [startPushLoop, stopPushLoop]);

  const disconnectWifi = useCallback(async () => {
    stopPushLoop();
    await receiverRef.current?.stop();
    receiverRef.current = null;
    await window.c4dportal.camera.stop();
    setLiveStream(null);
  }, [stopPushLoop]);

  const handleToggleConnection = () => {
    if (status === 'connected') {
      setStatus('disconnected');
      setDevice(null);
      setIsPaused(false);
      if (connectionType === 'wifi') void disconnectWifi();
      else void disconnectUsb();
    } else if (status === 'disconnected') {
      setStatus('connecting');
      if (connectionType === 'wifi') void connectWifi();
      else void connectUsb();
    }
  };

  const handleSettingChange = <K extends keyof StreamSettings>(key: K, value: StreamSettings[K]) => {
    // The CAMERA dropdown is the real front/rear switch control — picking a
    // different option actually tells the phone to switch, same command
    // either transport understands (see docs/protocol.md).
    if (key === 'cameraFacing' && value !== settings.cameraFacing && status === 'connected') {
      const facing = value as CameraFacing;
      if (connectionType === 'wifi') {
        void window.c4dportal.signaling.sendToPhone({ type: 'switch-camera', facing });
      } else {
        void window.c4dportal.usb.switchCamera(facing);
      }
    }
    if (key === 'flipHorizontal' || key === 'flipVertical') {
      const next = { ...settings, [key]: value };
      void window.c4dportal.camera.setFlip(next.flipHorizontal, next.flipVertical);
    }
    if (key === 'brightness' || key === 'contrast' || key === 'saturation') {
      const next = { ...settings, [key]: value };
      void window.c4dportal.camera.setAdjustments(next.brightness, next.contrast, next.saturation);
    }
    setSettings((prev) => ({ ...prev, [key]: value }));
  };

  const stats: StreamStats = {
    resolution: settings.resolution.split(' ').slice(0, 3).join(' '),
    fps: settings.fps,
    latencyMs: connectionType === 'usb' ? 8 : 24,
    codec: settings.videoFormat === 'H.264' ? 'H.264' : 'MJPEG',
    batteryPercent: device?.batteryPercent ?? 0,
  };

  return (
    <div className="app">
      <header className="app-header">
        <div className="app-header__brand">
          <span className="app-header__logo">📷</span>
          <div>
            <h1>C4D PORTAL</h1>
            <p>Use your Android device as a high quality webcam</p>
          </div>
        </div>
        <div className="app-header__status">
          <span className={`status-dot status-dot--${status}`} />
          {status === 'connected' ? 'Connected' : status === 'connecting' ? 'Connecting…' : 'Disconnected'}
        </div>
      </header>

      <div className="app-body">
        <div className="app-sidebar">
          <ConnectionPanel
            selected={connectionType}
            status={status}
            wifiAddress={device?.ipAddress ?? '192.168.1.105'}
            onSelect={(type) => {
              if (status !== 'disconnected') return;
              setConnectionType(type);
            }}
          />
          <DevicePanel device={device} status={status} />
          <ToolsPanel
            onOpenSettings={() => {}}
            onOpenFilters={() => {}}
            onOpenAbout={() => {}}
          />
          <PairingPanel pairingCode="482-119" />
        </div>

        <PreviewPanel
          status={status}
          settings={settings}
          stats={stats}
          isPaused={isPaused}
          liveStream={liveStream}
          usbFrame={usbFrame}
          onTogglePause={() => setIsPaused((p) => !p)}
          onToggleConnection={handleToggleConnection}
        />

        <SettingsPanel settings={settings} onChange={handleSettingChange} />
      </div>

      <footer className="app-footer">
        <span className="app-footer__version"><span className="status-dot status-dot--connected" /> C4D Portal v0.1.0</span>
        <span>Optimized for OBS, Streamlabs, Zoom, Discord &amp; more</span>
        <span>🔄 Check for Updates</span>
      </footer>
    </div>
  );
}
