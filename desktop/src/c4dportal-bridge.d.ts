export interface SignalingMessage {
  type: 'offer' | 'answer' | 'ice' | 'switch-camera';
  sdp?: string;
  candidate?: RTCIceCandidateInit;
  facing?: 'rear' | 'front';
}

export interface C4DPortalBridge {
  platform: string;
  signaling: {
    start: () => Promise<boolean>;
    stop: () => Promise<boolean>;
    sendToPhone: (msg: SignalingMessage) => Promise<boolean>;
    onPhoneConnected: (cb: () => void) => () => void;
    onPhoneDisconnected: (cb: () => void) => () => void;
    onMessageFromPhone: (cb: (msg: SignalingMessage) => void) => () => void;
  };
  camera: {
    create: (name?: string) => Promise<{ ok: boolean; error?: string }>;
    start: () => Promise<{ ok: boolean; error?: string }>;
    stop: () => Promise<boolean>;
    pushFrame: (buffer: Uint8Array, width: number, height: number) => void;
    setFlip: (horizontal: boolean, vertical: boolean) => Promise<boolean>;
    setAdjustments: (brightness: number, contrast: number, saturation: number) => Promise<boolean>;
  };
  usb: {
    start: () => Promise<boolean>;
    stop: () => Promise<boolean>;
    switchCamera: (facing: 'rear' | 'front') => Promise<boolean>;
    onPhoneConnected: (cb: () => void) => () => void;
    onPhoneDisconnected: (cb: () => void) => () => void;
    onError: (cb: (message: string) => void) => () => void;
    onFrame: (cb: (jpegBuffer: Uint8Array, width: number, height: number) => void) => () => void;
  };
}

declare global {
  interface Window {
    c4dportal: C4DPortalBridge;
  }
}
