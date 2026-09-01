#include "virtual_camera.h"

#include <mferror.h>

using Microsoft::WRL::ComPtr;

namespace {
std::wstring HrToMessage(HRESULT hr) {
  wchar_t buf[256];
  swprintf_s(buf, L"HRESULT 0x%08lX", static_cast<unsigned long>(hr));
  return buf;
}
}  // namespace

VirtualCamera::VirtualCamera() = default;
VirtualCamera::~VirtualCamera() { Stop(); }

bool VirtualCamera::Create(const std::wstring& friendlyName, std::wstring* errorOut) {
  if (created_) return true;

  HRESULT hr = MFStartup(MF_VERSION);
  if (FAILED(hr)) {
    if (errorOut) *errorOut = L"MFStartup failed: " + HrToMessage(hr);
    return false;
  }
  mf_started_ = true;

  ComPtr<IMFVirtualCamera> camera;
  // MFVirtualCameraType_SoftwareCameraSource: a purely software-fed device
  // (no physical sensor backing it) — matches C4D Portal's model of
  // pushing decoded phone frames in from user-mode.
  // MFVirtualCameraLifetime_Session: unregisters automatically when this
  // process exits, so a crash doesn't leave a stale device registered.
  hr = MFCreateVirtualCamera(
      MFVirtualCameraType_SoftwareCameraSource,
      MFVirtualCameraLifetime_Session,
      MFVirtualCameraAccess_CurrentUser,
      friendlyName.c_str(),
      // source id — must be a CLSID-formatted GUID string, not an
      // arbitrary name (confirmed by trial: a plain string yields
      // CO_E_CLASSSTRING). Must match the CLSID registered by
      // C4DPortalMediaSource.dll (native/media-source).
      L"{7B4F1A2E-9C3D-4E8A-B6F0-1A2B3C4D5E6F}",
      nullptr, 0,
      &camera);

  if (FAILED(hr)) {
    if (errorOut) *errorOut = L"MFCreateVirtualCamera failed: " + HrToMessage(hr);
    return false;
  }

  virtual_camera_ = camera;
  created_ = true;
  return true;
}

bool VirtualCamera::Start(std::wstring* errorOut) {
  if (!created_ || !virtual_camera_) {
    if (errorOut) *errorOut = L"Start() called before a successful Create()";
    return false;
  }
  HRESULT hr = virtual_camera_->Start(nullptr);
  running_ = SUCCEEDED(hr);
  if (!running_ && errorOut) *errorOut = L"IMFVirtualCamera::Start failed: " + HrToMessage(hr);
  return running_;
}

bool VirtualCamera::PushFrame(const uint8_t* data, size_t length, uint32_t width, uint32_t height) {
  if (!running_) return false;
  if (!shared_buffer_.OpenOrCreate()) return false;
  if (length < static_cast<size_t>(width) * height * 4) return false;
  return shared_buffer_.Write(data, width, height);
}

void VirtualCamera::Stop() {
  if (virtual_camera_) {
    virtual_camera_->Stop();
    virtual_camera_->Shutdown();
    virtual_camera_.Reset();
  }
  running_ = false;
  created_ = false;
  if (mf_started_) {
    MFShutdown();
    mf_started_ = false;
  }
}
