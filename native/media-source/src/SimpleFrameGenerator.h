#pragma once
#ifndef SIMPLE_FRAME_GENERATOR_H
#define SIMPLE_FRAME_GENERATOR_H

#include <memory>

#include "SharedFrameBuffer.h"

// Fills a Media Foundation sample buffer for one video frame. Pulls the most
// recent frame C4D Portal's desktop app pushed via SharedFrameBuffer; if none
// has arrived yet (not paired, or the phone hasn't started sending), falls
// back to a solid-color placeholder so the camera device still shows
// *something* rather than failing sample requests outright.
class SimpleFrameGenerator {
 public:
  SimpleFrameGenerator() = default;
  ~SimpleFrameGenerator() {}

  HRESULT Initialize(_In_ IMFMediaType* pMediaType);

  HRESULT CreateFrame(
      _Inout_updates_bytes_(len) BYTE* pBuf,
      _In_ DWORD len,
      _In_ LONG pitch);

  static void RGB24ToYUY2(int R, int G, int B, BYTE* pY, BYTE* pU, BYTE* pV);
  static void RGB24ToY(int R, int G, int B, BYTE* pY);
  static void RGB32ToNV12(BYTE RGB1[8], BYTE RGB2[8], BYTE* pY1, BYTE* pY2, BYTE* pUV);
  static HRESULT RGB32ToNV12Frame(_Inout_updates_bytes_(len) BYTE* pbBuff, ULONG cbBuff,
                                   long stride, UINT width, UINT height, BYTE* pbBuffOut,
                                   ULONG cbBuffOut, long strideOut);

 private:
  HRESULT _FillFromSharedBuffer(_Inout_updates_bytes_(len) BYTE* pBuf, DWORD len, LONG pitch);
  HRESULT _FillPlaceholder(_Inout_updates_bytes_(len) BYTE* pBuf, DWORD len, LONG pitch);

  UINT32 m_width = 0;
  UINT32 m_height = 0;
  GUID m_subType = GUID_NULL;
  SharedFrameBuffer m_sharedBuffer;
  uint64_t m_lastFrameSeqSeen = 0;
  // Scratch buffer sized for the shared buffer's max frame — reused across
  // calls to avoid an allocation on every sample request.
  std::unique_ptr<uint8_t[]> m_scratchBgra;
};

#endif
