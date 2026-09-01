#include "pch.h"

#include <algorithm>

HRESULT SimpleFrameGenerator::Initialize(_In_ IMFMediaType* pMediaType) {
  RETURN_HR_IF_NULL(E_INVALIDARG, pMediaType);

  RETURN_IF_FAILED(pMediaType->GetGUID(MF_MT_SUBTYPE, &m_subType));
  if (m_subType != MFVideoFormat_RGB32 && m_subType != MFVideoFormat_NV12) {
    return MF_E_UNSUPPORTED_FORMAT;
  }
  MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &m_width, &m_height);

  m_sharedBuffer.OpenOrCreate();
  m_scratchBgra = std::make_unique<uint8_t[]>(static_cast<size_t>(m_width) * m_height * 4);

  return S_OK;
}

HRESULT SimpleFrameGenerator::CreateFrame(
    _Inout_updates_bytes_(len) BYTE* pBuf,
    _In_ DWORD len,
    _In_ LONG pitch) {
  if (m_subType == MFVideoFormat_RGB32) {
    return _FillFromSharedBuffer(pBuf, len, pitch);
  } else if (m_subType == MFVideoFormat_NV12) {
    DWORD rgbLen = m_width * m_height * 4;
    auto rgbScratch = std::make_unique<uint8_t[]>(rgbLen);
    RETURN_IF_FAILED(_FillFromSharedBuffer(rgbScratch.get(), rgbLen, m_width * 4));
    RETURN_IF_FAILED(RGB32ToNV12Frame(rgbScratch.get(), rgbLen, m_width * 4, m_width, m_height,
                                       pBuf, len, pitch));
    return S_OK;
  }
  return MF_E_UNSUPPORTED_FORMAT;
}

//////////////////////////////////////////////////
// private

HRESULT SimpleFrameGenerator::_FillFromSharedBuffer(
    _Inout_updates_bytes_(len) BYTE* pBuf, DWORD len, LONG pitch) {
  RETURN_HR_IF_NULL(E_INVALIDARG, pBuf);
  if (len < static_cast<DWORD>(abs(pitch)) * m_height) {
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  uint32_t srcWidth = 0, srcHeight = 0;
  bool gotFrame = m_sharedBuffer.Read(m_scratchBgra.get(),
                                       static_cast<size_t>(m_width) * m_height * 4, &srcWidth,
                                       &srcHeight, &m_lastFrameSeqSeen);

  // Either no frame has arrived yet, or the sizes don't match what the
  // shared-buffer read populated this call (Read() only writes on success,
  // so on failure m_scratchBgra may hold a stale or zeroed frame) — in
  // that case, show a placeholder rather than garbage.
  if (!gotFrame || srcWidth == 0 || srcHeight == 0) {
    return _FillPlaceholder(pBuf, len, pitch);
  }

  // Nearest-neighbor scale from the pushed frame's resolution into the
  // negotiated stream resolution (m_width x m_height), preserving the
  // source's aspect ratio with letterbox/pillarbox bars rather than
  // stretching to fill — the phone's camera and the negotiated stream
  // size rarely share an exact aspect ratio (e.g. a 4:3 capture into a
  // 16:9 stream), and naive stretch-to-fill visibly distorts the image.
  double scale = (std::min)(static_cast<double>(m_width) / srcWidth,
                             static_cast<double>(m_height) / srcHeight);
  uint32_t scaledWidth = static_cast<uint32_t>(srcWidth * scale);
  uint32_t scaledHeight = static_cast<uint32_t>(srcHeight * scale);
  uint32_t offsetX = (m_width - scaledWidth) / 2;
  uint32_t offsetY = (m_height - scaledHeight) / 2;

  for (UINT32 y = 0; y < m_height; y++) {
    uint32_t* dstRow = reinterpret_cast<uint32_t*>(pBuf + static_cast<size_t>(y) * pitch);

    if (y < offsetY || y >= offsetY + scaledHeight) {
      for (UINT32 x = 0; x < m_width; x++) dstRow[x] = 0;
      continue;
    }

    uint32_t srcY = ((y - offsetY) * srcHeight) / scaledHeight;
    const uint8_t* srcRow = m_scratchBgra.get() + static_cast<size_t>(srcY) * srcWidth * 4;

    for (UINT32 x = 0; x < m_width; x++) {
      if (x < offsetX || x >= offsetX + scaledWidth) {
        dstRow[x] = 0;
        continue;
      }
      uint32_t srcX = ((x - offsetX) * srcWidth) / scaledWidth;
      const uint8_t* px = srcRow + static_cast<size_t>(srcX) * 4;
      // BGRA in memory -> 0x00RRGGBB for RGB32 (MFVideoFormat_RGB32 is
      // little-endian BGRX per pixel, i.e. byte order B,G,R,X).
      dstRow[x] = (px[3] << 24) | (px[2] << 16) | (px[1] << 8) | px[0];
    }
  }

  return S_OK;
}

HRESULT SimpleFrameGenerator::_FillPlaceholder(
    _Inout_updates_bytes_(len) BYTE* pBuf, DWORD len, LONG pitch) {
  RETURN_HR_IF_NULL(E_INVALIDARG, pBuf);
  if (len < static_cast<DWORD>(abs(pitch)) * m_height) {
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  // Dark gray, matching the desktop app's "no live source" placeholder.
  for (UINT32 r = 0; r < m_height; r++) {
    uint32_t* p = reinterpret_cast<uint32_t*>(pBuf + static_cast<size_t>(r) * pitch);
    for (UINT32 c = 0; c < m_width; c++) {
      p[c] = 0x00202830;
    }
  }
  return S_OK;
}

//////////////////////////////////////////////////
// pixel format converters (from Microsoft's Windows-Camera VirtualCamera sample)

void SimpleFrameGenerator::RGB24ToYUY2(int R, int G, int B, BYTE* pY, BYTE* pU, BYTE* pV) {
  *pY = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
  *pU = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
  *pV = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
}

void SimpleFrameGenerator::RGB24ToY(int R, int G, int B, BYTE* pY) {
  *pY = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
}

void SimpleFrameGenerator::RGB32ToNV12(BYTE RGB1[8], BYTE RGB2[8], BYTE* pY1, BYTE* pY2, BYTE* pUV) {
  RGB24ToYUY2(RGB1[2], RGB1[1], RGB1[0], pY1, pUV, pUV + 1);
  RGB24ToY(RGB1[6], RGB1[5], RGB1[4], pY1 + 1);
  RGB24ToYUY2(RGB2[2], RGB2[1], RGB2[0], pY2, pUV, pUV + 1);
  RGB24ToY(RGB2[6], RGB2[5], RGB2[4], pY2 + 1);
}

HRESULT SimpleFrameGenerator::RGB32ToNV12Frame(
    _Inout_updates_bytes_(len) BYTE* pbBuff, ULONG cbBuff, long stride, UINT width, UINT height,
    BYTE* pbBuffOut, ULONG cbBuffOut, long strideOut) {
  RETURN_HR_IF(E_UNEXPECTED, width * 4 * height > cbBuff);
  RETURN_HR_IF(E_UNEXPECTED, static_cast<ULONG>(width * 1.5 * height) > cbBuffOut);
  RETURN_HR_IF_NULL(E_INVALIDARG, pbBuff);
  RETURN_HR_IF_NULL(E_INVALIDARG, pbBuffOut);

  for (DWORD h = 0; h < height - 1; h += 2) {
    BYTE* pRGB1 = h * stride + pbBuff;
    BYTE* pRGB2 = (h + 1) * stride + pbBuff;
    BYTE* pY1 = h * strideOut + pbBuffOut;
    BYTE* pY2 = (h + 1) * strideOut + pbBuffOut;
    BYTE* pUV = (h / 2 + height) * strideOut + pbBuffOut;

    for (DWORD w = 0; w < width; w += 2) {
      RGB32ToNV12(pRGB1, pRGB2, pY1, pY2, pUV);
      pRGB1 += 8;
      pRGB2 += 8;
      pY1 += 2;
      pY2 += 2;
      pUV += 2;
    }
  }

  return S_OK;
}
