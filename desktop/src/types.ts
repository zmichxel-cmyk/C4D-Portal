export type ConnectionType = 'usb' | 'wifi';

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected';

export interface DeviceInfo {
  name: string;
  osVersion: string;
  batteryPercent: number;
  ipAddress?: string;
}

export type CameraFacing = 'rear' | 'front';

export interface StreamSettings {
  cameraFacing: CameraFacing;
  resolution: string;
  fps: number;
  videoFormat: string;
  colorSpace: string;
  brightness: number;
  contrast: number;
  saturation: number;
  sharpness: number;
  whiteBalanceMode: string;
  lowLatencyMode: boolean;
  flipHorizontal: boolean;
  flipVertical: boolean;
  antiFlicker: string;
}

export interface StreamStats {
  resolution: string;
  fps: number;
  latencyMs: number;
  codec: string;
  batteryPercent: number;
}
