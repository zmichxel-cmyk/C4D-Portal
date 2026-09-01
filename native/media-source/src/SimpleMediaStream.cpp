#include "pch.h"

// Default negotiated resolution/framerate — the settings panel's resolution
// picker will drive this once the desktop app renegotiates media types
// through IMFMediaTypeHandler (build order step 6); for now streams open at
// this fixed size and SimpleFrameGenerator scales whatever the phone
// actually sends to fit it.
#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

namespace winrt::WindowsSample::implementation {

HRESULT SimpleMediaStream::Initialize(_In_ SimpleMediaSource* pSource, _In_ DWORD dwStreamId,
                                       _In_ MFSampleAllocatorUsage allocatorUsage) {
  winrt::slim_lock_guard lock(m_Lock);

  wil::com_ptr_nothrow<IMFMediaTypeHandler> spTypeHandler;

  RETURN_HR_IF_NULL(E_INVALIDARG, pSource);
  m_parent = pSource;
  m_dwStreamId = dwStreamId;
  m_allocatorUsage = allocatorUsage;

  const uint32_t NUM_MEDIATYPES = 2;
  wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFMediaType>> mediaTypeList =
      wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFMediaType>>(NUM_MEDIATYPES);

  wil::com_ptr_nothrow<IMFMediaType> spMediaType;
  RETURN_IF_FAILED(MFCreateMediaType(&spMediaType));
  spMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  spMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  spMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  spMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  MFSetAttributeSize(spMediaType.get(), MF_MT_FRAME_SIZE, DEFAULT_WIDTH, DEFAULT_HEIGHT);
  MFSetAttributeRatio(spMediaType.get(), MF_MT_FRAME_RATE, 30, 1);
  spMediaType->SetUINT32(MF_MT_AVG_BITRATE,
                          static_cast<uint32_t>(DEFAULT_WIDTH * 1.5 * DEFAULT_HEIGHT * 8 * 30));
  MFSetAttributeRatio(spMediaType.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  mediaTypeList[0] = spMediaType.detach();

  RETURN_IF_FAILED(MFCreateMediaType(&spMediaType));
  spMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  spMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
  spMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  spMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  MFSetAttributeSize(spMediaType.get(), MF_MT_FRAME_SIZE, DEFAULT_WIDTH, DEFAULT_HEIGHT);
  MFSetAttributeRatio(spMediaType.get(), MF_MT_FRAME_RATE, 30, 1);
  spMediaType->SetUINT32(MF_MT_AVG_BITRATE,
                          static_cast<uint32_t>(DEFAULT_WIDTH * DEFAULT_HEIGHT * 4 * 8 * 30));
  MFSetAttributeRatio(spMediaType.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  mediaTypeList[1] = spMediaType.detach();

  RETURN_IF_FAILED(MFCreateAttributes(&m_spAttributes, 10));
  RETURN_IF_FAILED(_SetStreamAttributes(m_spAttributes.get()));
  RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));

  RETURN_IF_FAILED(MFCreateStreamDescriptor(m_dwStreamId, NUM_MEDIATYPES, mediaTypeList.get(), &m_spStreamDesc));
  RETURN_IF_FAILED(m_spStreamDesc->GetMediaTypeHandler(&spTypeHandler));
  RETURN_IF_FAILED(spTypeHandler->SetCurrentMediaType(mediaTypeList[0]));
  RETURN_IF_FAILED(_SetStreamDescriptorAttributes(m_spStreamDesc.get()));

  return S_OK;
}

// IMFMediaEventGenerator
IFACEMETHODIMP SimpleMediaStream::BeginGetEvent(_In_ IMFAsyncCallback* pCallback, _In_ IUnknown* punkState) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaStream::EndGetEvent(_In_ IMFAsyncResult* pResult, _COM_Outptr_ IMFMediaEvent** ppEvent) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaStream::GetEvent(_In_ DWORD dwFlags, _COM_Outptr_ IMFMediaEvent** ppEvent) {
  wil::com_ptr_nothrow<IMFMediaEventQueue> spQueue;
  {
    winrt::slim_lock_guard lock(m_Lock);
    RETURN_IF_FAILED(_CheckShutdownRequiresLock());
    spQueue = m_spEventQueue;
  }
  RETURN_IF_FAILED(spQueue->GetEvent(dwFlags, ppEvent));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaStream::QueueEvent(_In_ MediaEventType eventType, _In_ REFGUID guidExtendedType,
                                              _In_ HRESULT hrStatus, _In_opt_ PROPVARIANT const* pvValue) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(eventType, guidExtendedType, hrStatus, pvValue));
  return S_OK;
}

// IMFMediaStream
IFACEMETHODIMP SimpleMediaStream::GetMediaSource(_COM_Outptr_ IMFMediaSource** ppMediaSource) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
  *ppMediaSource = nullptr;
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_parent.copy_to(ppMediaSource));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaStream::GetStreamDescriptor(_COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
  *ppStreamDescriptor = nullptr;
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_HR_IF_NULL(E_UNEXPECTED, m_spStreamDesc);
  RETURN_IF_FAILED(m_spStreamDesc.copy_to(ppStreamDescriptor));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaStream::RequestSample(_In_ IUnknown* pToken) {
  winrt::slim_lock_guard lock(m_Lock);
  wil::com_ptr_nothrow<IMFSample> sample;
  wil::com_ptr_nothrow<IMFMediaBuffer> outputBuffer;
  LONG pitch = 0;
  BYTE* bufferStart = nullptr;
  DWORD bufferLength = 0;
  BYTE* pbuf = nullptr;
  wil::com_ptr_nothrow<IMF2DBuffer2> buffer2D;

  RETURN_IF_FAILED(_CheckShutdownRequiresLock());

  if (m_streamState != MF_STREAM_STATE_RUNNING) {
    return MF_E_INVALIDREQUEST;
  }

  RETURN_IF_FAILED(m_spSampleAllocator->AllocateSample(&sample));
  RETURN_IF_FAILED(sample->GetBufferByIndex(0, &outputBuffer));
  RETURN_IF_FAILED(outputBuffer->QueryInterface(IID_PPV_ARGS(&buffer2D)));
  RETURN_IF_FAILED(buffer2D->Lock2DSize(MF2DBuffer_LockFlags_Write, &pbuf, &pitch, &bufferStart, &bufferLength));

  RETURN_IF_FAILED(m_spFrameGenerator->CreateFrame(pbuf, bufferLength, pitch));
  RETURN_IF_FAILED(buffer2D->Unlock2D());

  RETURN_IF_FAILED(sample->SetSampleTime(MFGetSystemTime()));
  RETURN_IF_FAILED(sample->SetSampleDuration(333333));
  if (pToken != nullptr) {
    RETURN_IF_FAILED(sample->SetUnknown(MFSampleExtension_Token, pToken));
  }
  RETURN_IF_FAILED(m_spEventQueue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample.get()));

  return S_OK;
}

// IMFMediaStream2
IFACEMETHODIMP SimpleMediaStream::SetStreamState(MF_STREAM_STATE state) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());

  if (m_streamState == state) return S_OK;

  switch (state) {
    case MF_STREAM_STATE_PAUSED:
      if (m_streamState != MF_STREAM_STATE_RUNNING) return MF_E_INVALID_STATE_TRANSITION;
      m_streamState = MF_STREAM_STATE_PAUSED;
      break;
    case MF_STREAM_STATE_RUNNING:
      RETURN_IF_FAILED(StartInternal(false, nullptr));
      break;
    case MF_STREAM_STATE_STOPPED:
      RETURN_IF_FAILED(StopInternal(false));
      break;
    default:
      return MF_E_INVALID_STATE_TRANSITION;
  }
  return S_OK;
}

IFACEMETHODIMP SimpleMediaStream::GetStreamState(_Out_ MF_STREAM_STATE* pState) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_HR_IF_NULL(E_INVALIDARG, pState);
  *pState = m_streamState;
  return S_OK;
}

HRESULT SimpleMediaStream::Start(_In_ IMFMediaType* pMediaType) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_INVALIDARG, pMediaType);
  if (m_spMediaType == nullptr) {
    m_spMediaType = pMediaType;
  }
  m_bSelected = true;
  RETURN_IF_FAILED(StartInternal(true, pMediaType));
  return S_OK;
}

HRESULT SimpleMediaStream::Stop(_In_ bool bSendEvent) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  m_bSelected = false;
  RETURN_IF_FAILED(StopInternal(bSendEvent));
  return S_OK;
}

HRESULT SimpleMediaStream::Shutdown() {
  winrt::slim_lock_guard lock(m_Lock);
  m_bIsShutdown = true;
  m_parent.reset();
  if (m_spEventQueue != nullptr) {
    m_spEventQueue->Shutdown();
    m_spEventQueue.reset();
  }
  m_spAttributes.reset();
  m_spStreamDesc.reset();
  m_streamState = MF_STREAM_STATE_STOPPED;
  return S_OK;
}

HRESULT SimpleMediaStream::SetSampleAllocator(IMFVideoSampleAllocator* pAllocator) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  if (m_streamState == MF_STREAM_STATE_RUNNING) {
    return MF_E_INVALIDREQUEST;
  }
  m_spSampleAllocator.reset();
  m_spSampleAllocator = pAllocator;
  return S_OK;
}

HRESULT SimpleMediaStream::_CheckShutdownRequiresLock() {
  if (m_bIsShutdown) return MF_E_SHUTDOWN;
  if (m_spEventQueue == nullptr) return E_UNEXPECTED;
  return S_OK;
}

HRESULT SimpleMediaStream::_SetStreamAttributes(_In_ IMFAttributes* pAttributeStore) {
  RETURN_HR_IF_NULL(E_INVALIDARG, pAttributeStore);
  RETURN_IF_FAILED(pAttributeStore->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE));
  RETURN_IF_FAILED(pAttributeStore->SetUINT32(MF_DEVICESTREAM_STREAM_ID, m_dwStreamId));
  RETURN_IF_FAILED(pAttributeStore->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1));
  RETURN_IF_FAILED(pAttributeStore->SetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES, MFFrameSourceTypes::MFFrameSourceTypes_Color));
  return S_OK;
}

HRESULT SimpleMediaStream::_SetStreamDescriptorAttributes(_In_ IMFAttributes* pAttributeStore) {
  return _SetStreamAttributes(pAttributeStore);
}

HRESULT SimpleMediaStream::StartInternal(bool bSendEvent, IMFMediaType* pNewMediaType) {
  BOOL bMatch = FALSE;
  if (m_spMediaType && pNewMediaType) {
    (void)m_spMediaType->Compare(pNewMediaType, MF_ATTRIBUTES_MATCH_ALL_ITEMS, &bMatch);
    if (!bMatch) {
      m_spMediaType = pNewMediaType;
    }
  }

  if ((m_streamState != MF_STREAM_STATE_RUNNING) || !bMatch) {
    if (m_allocatorUsage == MFSampleAllocatorUsage_UsesProvidedAllocator) {
      RETURN_HR_IF_NULL(E_POINTER, m_spSampleAllocator);
    } else {
      RETURN_IF_FAILED(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&m_spSampleAllocator)));
    }

    RETURN_IF_FAILED(m_spSampleAllocator->InitializeSampleAllocator(10, m_spMediaType.get()));
    if (m_spFrameGenerator == nullptr) {
      m_spFrameGenerator = std::make_unique<SimpleFrameGenerator>();
      RETURN_IF_NULL_ALLOC(m_spFrameGenerator);
    }
    RETURN_IF_FAILED(m_spFrameGenerator->Initialize(m_spMediaType.get()));
  }

  if (bSendEvent) {
    RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr));
  }
  m_streamState = MF_STREAM_STATE_RUNNING;
  return S_OK;
}

HRESULT SimpleMediaStream::StopInternal(bool bSendEvent) {
  m_streamState = MF_STREAM_STATE_STOPPED;
  if (bSendEvent) {
    RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr));
  }
  return S_OK;
}
}  // namespace winrt::WindowsSample::implementation
