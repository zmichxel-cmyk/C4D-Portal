#pragma once
#include <mfapi.h>
#include <mfidl.h>
#include <mfvirtualcamera.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

#include "SharedFrameBuffer.h"

// Wraps the Windows Media Foundation Virtual Camera API (IMFVirtualCamera,
// available Windows 10 2004+/Windows 11). Registers a system-visible camera
// device named "C4D Portal" that any app (OBS, Streamlabs, Zoom, Teams, the
// Windows Camera app, ...) can pick from its camera list.
//
// NOTE: Create()/Start() register and start the OS-visible device — that
// part is real. PushFrame() writes into SharedFrameBuffer, which
// C4DPortalMediaSource.dll (a separate COM server, loaded by the Windows
// FrameServer service) reads from to actually serve frames — see
// docs/protocol.md.
class VirtualCamera {
 public:
  VirtualCamera();
  ~VirtualCamera();

  // Registers the "C4D Portal" device with the OS. Requires the calling
  // process to be elevated (admin) the first time it runs.
  bool Create(const std::wstring& friendlyName, std::wstring* errorOut);

  // Starts the device so consuming apps can open it.
  bool Start(std::wstring* errorOut);

  // Pushes one decoded BGRA frame (width * height * 4 bytes) into the
  // camera's active sample stream.
  bool PushFrame(const uint8_t* data, size_t length, uint32_t width, uint32_t height);

  // Stops and unregisters the device.
  void Stop();

 private:
  bool mf_started_ = false;
  bool created_ = false;
  bool running_ = false;
  Microsoft::WRL::ComPtr<IMFVirtualCamera> virtual_camera_;
  SharedFrameBuffer shared_buffer_;
};
