// Precompiled header for C4DPortalMediaSource.dll — trimmed from Microsoft's
// Windows-Camera VirtualCamera sample (VirtualCameraMediaSource/pch.h) to
// only what the "Synthetic" (SimpleMediaSource) path needs: no HW/Augmented
// camera wrapping, no WinRT ApplicationModel PFN lookup.
#pragma once
#include <unknwn.h>
#include <windows.h>
#include <propvarutil.h>

#include <ole2.h>  // must come before winrt headers
#include <initguid.h>
#include <Ks.h>
#include <ksproxy.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <mfvirtualcamera.h>

#define RESULT_DIAGNOSTICS_LEVEL 4

#include <wil\cppwinrt.h>  // must be before the first C++/WinRT header
#include <wil\result.h>
#include <wil\com.h>

#include "SharedFrameBuffer.h"
#include "SimpleFrameGenerator.h"
#include "SimpleMediaSource.h"
#include "SimpleMediaStream.h"
#include "VirtualCameraMediaSource.h"
#include "VirtualCameraMediaSourceActivate.h"

#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "Mfsensorgroup")

inline void DebugPrint(LPCWSTR szFormat, ...) {
  WCHAR szBuffer[512] = {0};
  va_list pArgs;
  va_start(pArgs, szFormat);
  StringCbVPrintfW(szBuffer, sizeof(szBuffer), szFormat, pArgs);
  va_end(pArgs);
  OutputDebugStringW(szBuffer);
}

#define DEBUG_MSG(msg, ...)                                     \
  {                                                              \
    DebugPrint(L"[%S@%d] ", __FUNCTION__, __LINE__);             \
    DebugPrint(msg, __VA_ARGS__);                                 \
    DebugPrint(L"\n");                                            \
  }

namespace wilEx {
template <typename T>
wil::unique_cotaskmem_array_ptr<T> make_unique_cotaskmem_array(size_t numOfElements) {
  wil::unique_cotaskmem_array_ptr<T> arr;
  size_t cb = sizeof(typename wil::details::element_traits<T>::type) * numOfElements;
  void* ptr = ::CoTaskMemAlloc(cb);
  if (ptr != nullptr) {
    ZeroMemory(ptr, cb);
    arr.reset(reinterpret_cast<typename wil::details::element_traits<T>::type*>(ptr), numOfElements);
  }
  return arr;
}
};  // namespace wilEx

namespace winrt {
template <>
bool is_guid_of<IMFMediaSourceEx>(guid const& id) noexcept;
template <>
bool is_guid_of<IMFMediaStream2>(guid const& id) noexcept;
template <>
bool is_guid_of<IMFActivate>(guid const& id) noexcept;
};  // namespace winrt
