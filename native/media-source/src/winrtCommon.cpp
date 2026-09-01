#include "pch.h"

// C++/WinRT needs explicit is_guid_of specializations for the classic-COM
// (non-WinRT-metadata) interfaces implemented via winrt::implements below,
// since it can't derive the vtable/IID mapping for these from Windows
// metadata the way it does for WinRT types. Same as Microsoft's
// Windows-Camera VirtualCamera sample.
WINRT_EXPORT namespace winrt {
template <>
bool is_guid_of<IMFMediaSourceEx>(guid const& id) noexcept {
  return is_guid_of<IMFMediaSourceEx, IMFMediaSource, IMFMediaEventGenerator>(id);
}

template <>
bool is_guid_of<IMFMediaStream2>(guid const& id) noexcept {
  return is_guid_of<IMFMediaStream2, IMFMediaStream, IMFMediaEventGenerator>(id);
}

template <>
bool is_guid_of<IMFActivate>(guid const& id) noexcept {
  return is_guid_of<IMFActivate, IMFAttributes>(id);
}
}  // namespace winrt
