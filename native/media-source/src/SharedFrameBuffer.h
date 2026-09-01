#pragma once
#include <windows.h>
#include <sddl.h>
#include <cstdint>
#pragma comment(lib, "advapi32.lib")

// Cross-process bridge between the Electron/Node process (writer, via the
// c4dportal_virtual_camera addon's PushFrame) and C4DPortalMediaSource.dll
// (reader, loaded inside the Windows Frame Server service process when an
// app opens the "C4D Portal" camera). A named file mapping + named mutex are
// used because the two sides run in different processes but the same user
// session — see docs/protocol.md "Frame delivery to the virtual camera".
//
// Layout: [Header][BGRA pixel data, up to kMaxWidth * kMaxHeight * 4 bytes]
class SharedFrameBuffer {
 public:
  static constexpr uint32_t kMaxWidth = 3840;
  static constexpr uint32_t kMaxHeight = 2160;
  static constexpr size_t kMaxFrameBytes = static_cast<size_t>(kMaxWidth) * kMaxHeight * 4;

  // Global\ (not Local\) because the writer (our Electron/Node process, in
  // the interactive user's session) and the reader (this DLL, loaded inside
  // the FrameServer service, which runs in session 0) are in different
  // Terminal Services sessions — Local\ objects are session-scoped and
  // invisible across that boundary.
  static constexpr wchar_t kMappingName[] = L"Global\\C4DPortalFrameBuffer";
  static constexpr wchar_t kMutexName[] = L"Global\\C4DPortalFrameBufferMutex";

#pragma pack(push, 1)
  struct Header {
    uint32_t width;
    uint32_t height;
    uint64_t frameSeq;  // incremented on every Write(); readers use this to
                         // detect a frame they haven't consumed yet.
  };
#pragma pack(pop)

  ~SharedFrameBuffer() { Close(); }

  // Creates the mapping if it doesn't exist yet, or opens the existing one.
  // Either side (writer or reader) can call this first.
  bool OpenOrCreate() {
    if (mapping_) return true;

    // The writer runs as the interactive user; the reader runs inside the
    // FrameServer service as NT AUTHORITY\LocalService — a different
    // security principal. Without an explicit permissive DACL, whichever
    // side creates the object first gets a default ACL that denies the
    // other side access. "D:(A;;GA;;;WD)" grants full access to Everyone,
    // which is fine here since this is purely local video-frame IPC.
    PSECURITY_DESCRIPTOR sd = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;WD)", SDDL_REVISION_1, &sd, nullptr);
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), sd, FALSE};

    size_t totalBytes = sizeof(Header) + kMaxFrameBytes;
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, sd ? &sa : nullptr, PAGE_READWRITE,
                                   static_cast<DWORD>(totalBytes >> 32),
                                   static_cast<DWORD>(totalBytes & 0xFFFFFFFF),
                                   kMappingName);
    bool createdNew = mapping_ && GetLastError() != ERROR_ALREADY_EXISTS;
    if (!mapping_ && GetLastError() == ERROR_ACCESS_DENIED) {
      // Someone else created it with a stricter DACL before this fix shipped
      // (e.g. FrameServer still holding a stale mapping) — fall back to
      // opening for the access level we actually need.
      mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, kMappingName);
    }
    if (sd) LocalFree(sd);
    if (!mapping_) return false;
    (void)createdNew;

    view_ = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, totalBytes);
    if (!view_) {
      CloseHandle(mapping_);
      mapping_ = nullptr;
      return false;
    }

    PSECURITY_DESCRIPTOR mutexSd = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;WD)", SDDL_REVISION_1, &mutexSd, nullptr);
    SECURITY_ATTRIBUTES mutexSa = {sizeof(SECURITY_ATTRIBUTES), mutexSd, FALSE};
    mutex_ = CreateMutexW(mutexSd ? &mutexSa : nullptr, FALSE, kMutexName);
    if (!mutex_ && GetLastError() == ERROR_ACCESS_DENIED) {
      mutex_ = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, kMutexName);
    }
    if (mutexSd) LocalFree(mutexSd);
    if (!mutex_) {
      UnmapViewOfFile(view_);
      view_ = nullptr;
      CloseHandle(mapping_);
      mapping_ = nullptr;
      return false;
    }

    return true;
  }

  void Close() {
    if (view_) { UnmapViewOfFile(view_); view_ = nullptr; }
    if (mapping_) { CloseHandle(mapping_); mapping_ = nullptr; }
    if (mutex_) { CloseHandle(mutex_); mutex_ = nullptr; }
  }

  // Writer side: called from the Electron process with a decoded BGRA frame.
  bool Write(const uint8_t* bgra, uint32_t width, uint32_t height) {
    if (!view_ || !mutex_) return false;
    size_t frameBytes = static_cast<size_t>(width) * height * 4;
    if (frameBytes == 0 || frameBytes > kMaxFrameBytes) return false;

    if (WaitForSingleObject(mutex_, 500) != WAIT_OBJECT_0) return false;

    auto* header = reinterpret_cast<Header*>(view_);
    uint8_t* pixels = reinterpret_cast<uint8_t*>(view_) + sizeof(Header);
    memcpy(pixels, bgra, frameBytes);
    header->width = width;
    header->height = height;
    header->frameSeq += 1;

    ReleaseMutex(mutex_);
    return true;
  }

  // Reader side: called from C4DPortalMediaSource.dll when serving a sample.
  // Returns false only if no frame has EVER been written — otherwise it
  // keeps returning the most recent frame (repeating it) even if it isn't
  // new since the last call. The consuming stream's declared framerate
  // (e.g. 30fps) is usually higher than however often the phone/desktop
  // actually pushes updates, so gating on "new frame only" made most
  // RequestSample calls fall back to the gray placeholder — visible as
  // flicker. Repeating the last frame is what real cameras/virtual cams do
  // between source updates.
  bool Read(uint8_t* outBgra, size_t outCapacity, uint32_t* outWidth, uint32_t* outHeight,
            uint64_t* inOutLastSeqSeen) {
    if (!view_ || !mutex_) return false;

    if (WaitForSingleObject(mutex_, 500) != WAIT_OBJECT_0) return false;

    auto* header = reinterpret_cast<Header*>(view_);
    bool hasNewFrame = header->frameSeq != 0;
    if (hasNewFrame) {
      size_t frameBytes = static_cast<size_t>(header->width) * header->height * 4;
      if (frameBytes <= outCapacity) {
        const uint8_t* pixels = reinterpret_cast<const uint8_t*>(view_) + sizeof(Header);
        memcpy(outBgra, pixels, frameBytes);
        *outWidth = header->width;
        *outHeight = header->height;
        *inOutLastSeqSeen = header->frameSeq;
      } else {
        hasNewFrame = false;
      }
    }

    ReleaseMutex(mutex_);
    return hasNewFrame;
  }

 private:
  HANDLE mapping_ = nullptr;
  HANDLE mutex_ = nullptr;
  void* view_ = nullptr;
};
