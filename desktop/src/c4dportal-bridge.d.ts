export interface SignalingMessage {
  type: 'offer' | 'answer' | 'ice';
  sdp?: string;
  candidate?: RTCIceCandidateInit;
}

export interface CamlinkBridge {
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
  };
  usb: {
    start: () => Promise<boolean>;
    stop: () => Promise<boolean>;
    onPhoneConnected: (cb: () => void) => () => void;
    onPhoneDisconnected: (cb: () => void) => () => void;
    onError: (cb: (message: string) => void) => () => void;
    onFrame: (cb: (jpegBuffer: Uint8Array, width: number, height: number) => void) => () => void;
  };
}

declare global {
  interface Window {
    c4dportal: CamlinkBridge;
  }
}
