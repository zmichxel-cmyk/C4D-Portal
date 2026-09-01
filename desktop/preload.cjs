const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('c4dportal', {
  platform: process.platform,

  signaling: {
    start: () => ipcRenderer.invoke('signal:start'),
    stop: () => ipcRenderer.invoke('signal:stop'),
    sendToPhone: (msg) => ipcRenderer.invoke('signal:send-to-phone', msg),
    onPhoneConnected: (cb) => {
      const listener = () => cb();
      ipcRenderer.on('signal:phone-connected', listener);
      return () => ipcRenderer.removeListener('signal:phone-connected', listener);
    },
    onPhoneDisconnected: (cb) => {
      const listener = () => cb();
      ipcRenderer.on('signal:phone-disconnected', listener);
      return () => ipcRenderer.removeListener('signal:phone-disconnected', listener);
    },
    onMessageFromPhone: (cb) => {
      const listener = (_event, msg) => cb(msg);
      ipcRenderer.on('signal:from-phone', listener);
      return () => ipcRenderer.removeListener('signal:from-phone', listener);
    },
  },

  camera: {
    create: (name) => ipcRenderer.invoke('camera:create', name),
    start: () => ipcRenderer.invoke('camera:start'),
    stop: () => ipcRenderer.invoke('camera:stop'),
    pushFrame: (buffer, width, height) => ipcRenderer.send('camera:push-frame', buffer, width, height),
    setFlip: (horizontal, vertical) => ipcRenderer.invoke('camera:set-flip', horizontal, vertical),
  },

  usb: {
    start: () => ipcRenderer.invoke('usb:start'),
    stop: () => ipcRenderer.invoke('usb:stop'),
    switchCamera: (facing) => ipcRenderer.invoke('usb:switch-camera', facing),
    onPhoneConnected: (cb) => {
      const listener = () => cb();
      ipcRenderer.on('usb:phone-connected', listener);
      return () => ipcRenderer.removeListener('usb:phone-connected', listener);
    },
    onPhoneDisconnected: (cb) => {
      const listener = () => cb();
      ipcRenderer.on('usb:phone-disconnected', listener);
      return () => ipcRenderer.removeListener('usb:phone-disconnected', listener);
    },
    onError: (cb) => {
      const listener = (_event, message) => cb(message);
      ipcRenderer.on('usb:error', listener);
      return () => ipcRenderer.removeListener('usb:error', listener);
    },
    onFrame: (cb) => {
      const listener = (_event, jpegBuffer, width, height) => cb(jpegBuffer, width, height);
      ipcRenderer.on('usb:frame', listener);
      return () => ipcRenderer.removeListener('usb:frame', listener);
    },
  },
});
