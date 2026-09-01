#include "pch.h"

namespace winrt::WindowsSample::implementation {

HRESULT SimpleMediaSource::Initialize(_In_ IMFAttributes* pAttributes) {
  winrt::slim_lock_guard lock(m_Lock);

  if (m_initalized) {
    return MF_E_ALREADY_INITIALIZED;
  }

  RETURN_IF_FAILED(_CreateSourceAttributes(pAttributes));
  RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));

  m_streamList = wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<SimpleMediaStream>>(NUM_STREAMS);
  RETURN_IF_NULL_ALLOC(m_streamList.get());

  wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFStreamDescriptor>> streamDescriptorList =
      wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFStreamDescriptor>>(NUM_STREAMS);

  for (unsigned int i = 0; i < NUM_STREAMS; i++) {
    auto ptr = winrt::make_self<SimpleMediaStream>();
    m_streamList[i] = ptr.detach();
    RETURN_IF_FAILED(m_streamList[i]->Initialize(this, i, MFSampleAllocatorUsage_UsesProvidedAllocator));
    RETURN_IF_FAILED(m_streamList[i]->GetStreamDescriptor(&streamDescriptorList[i]));
  }

  RETURN_IF_FAILED(MFCreatePresentationDescriptor(
      static_cast<DWORD>(m_streamList.size()), streamDescriptorList.get(), &m_spPresentationDescriptor));

  m_sourceState = SourceState::Stopped;
  m_initalized = true;

  return S_OK;
}

// IMFMediaEventGenerator methods.
IFACEMETHODIMP SimpleMediaSource::BeginGetEvent(_In_ IMFAsyncCallback* pCallback, _In_ IUnknown* punkState) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::EndGetEvent(_In_ IMFAsyncResult* pResult, _COM_Outptr_ IMFMediaEvent** ppEvent) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::GetEvent(DWORD dwFlags, _COM_Outptr_ IMFMediaEvent** ppEvent) {
  wil::com_ptr_nothrow<IMFMediaEventQueue> spQueue;
  {
    winrt::slim_lock_guard lock(m_Lock);
    RETURN_IF_FAILED(_CheckShutdownRequiresLock());
    spQueue = m_spEventQueue;
  }
  RETURN_IF_FAILED(spQueue->GetEvent(dwFlags, ppEvent));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::QueueEvent(MediaEventType eventType, REFGUID guidExtendedType,
                                              HRESULT hrStatus, _In_opt_ PROPVARIANT const* pvValue) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(eventType, guidExtendedType, hrStatus, pvValue));
  return S_OK;
}

// IMFMediaSource methods
IFACEMETHODIMP SimpleMediaSource::CreatePresentationDescriptor(
    _COM_Outptr_ IMFPresentationDescriptor** ppPresentationDescriptor) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_POINTER, ppPresentationDescriptor);
  *ppPresentationDescriptor = nullptr;
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_IF_FAILED(m_spPresentationDescriptor->Clone(ppPresentationDescriptor));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::GetCharacteristics(_Out_ DWORD* pdwCharacteristics) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_POINTER, pdwCharacteristics);
  *pdwCharacteristics = 0;
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::Pause() { return MF_E_INVALID_STATE_TRANSITION; }

IFACEMETHODIMP SimpleMediaSource::Shutdown() {
  winrt::slim_lock_guard lock(m_Lock);
  m_sourceState = SourceState::Shutdown;
  m_spAttributes.reset();
  m_spPresentationDescriptor.reset();

  if (m_spEventQueue != nullptr) {
    m_spEventQueue->Shutdown();
    m_spEventQueue.reset();
  }
  for (unsigned int i = 0; i < m_streamList.size(); i++) {
    m_streamList[i]->Shutdown();
  }
  m_streamList.reset();
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::Start(_In_ IMFPresentationDescriptor* pPresentationDescriptor,
                                         _In_opt_ const GUID* pguidTimeFormat,
                                         _In_ const PROPVARIANT* pvarStartPos) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());

  if (pPresentationDescriptor == nullptr || pvarStartPos == nullptr) {
    return E_INVALIDARG;
  }
  if (pguidTimeFormat != nullptr && *pguidTimeFormat != GUID_NULL) {
    return MF_E_UNSUPPORTED_TIME_FORMAT;
  }
  if (m_sourceState == SourceState::Invalid) {
    return MF_E_INVALID_STATE_TRANSITION;
  }

  DWORD count = 0;
  wil::unique_prop_variant startTime;
  RETURN_IF_FAILED(_ValidatePresentationDescriptor(pPresentationDescriptor));
  RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorCount(&count));
  RETURN_IF_FAILED(InitPropVariantFromInt64(MFGetSystemTime(), &startTime));

  for (unsigned int i = 0; i < count; i++) {
    BOOL selected = false;
    wil::com_ptr_nothrow<IMFStreamDescriptor> streamDesc;
    RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorByIndex(i, &selected, &streamDesc));

    DWORD streamId = 0;
    RETURN_IF_FAILED(streamDesc->GetStreamIdentifier(&streamId));

    DWORD streamIdx = 0;
    bool wasSelected = false;
    wil::com_ptr_nothrow<IMFStreamDescriptor> spLocalStreamDescriptor;
    RETURN_IF_FAILED(_GetStreamDescriptorByStreamId(streamId, &streamIdx, &wasSelected, &spLocalStreamDescriptor));

    if (selected) {
      RETURN_IF_FAILED(m_spPresentationDescriptor->SelectStream(streamIdx));

      wil::com_ptr_nothrow<IMFMediaTypeHandler> spMTHandler;
      wil::com_ptr_nothrow<IMFMediaType> spMediaType;
      RETURN_IF_FAILED(streamDesc->GetMediaTypeHandler(&spMTHandler));
      RETURN_IF_FAILED(spMTHandler->GetCurrentMediaType(&spMediaType));

      wil::com_ptr_nothrow<IUnknown> spunkStream;
      MediaEventType met = (wasSelected ? MEUpdatedStream : MENewStream);
      RETURN_IF_FAILED(m_streamList[streamIdx]->QueryInterface(IID_PPV_ARGS(&spunkStream)));
      RETURN_IF_FAILED(m_spEventQueue->QueueEventParamUnk(met, GUID_NULL, S_OK, spunkStream.get()));

      RETURN_IF_FAILED(m_streamList[streamIdx]->Start(spMediaType.get()));
    } else if (wasSelected) {
      RETURN_IF_FAILED(m_spPresentationDescriptor->DeselectStream(streamIdx));
      RETURN_IF_FAILED(m_streamList[streamIdx]->Stop(false));
    }
  }

  RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, &startTime));
  m_sourceState = SourceState::Started;
  return S_OK;
}

HRESULT SimpleMediaSource::_GetStreamDescriptorByStreamId(DWORD dwStreamId, DWORD* pdwStreamIdx,
                                                            bool* pSelected, IMFStreamDescriptor** ppStreamDescriptor) {
  RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
  *ppStreamDescriptor = nullptr;
  RETURN_HR_IF_NULL(E_POINTER, pdwStreamIdx);
  *pdwStreamIdx = 0;
  RETURN_HR_IF_NULL(E_POINTER, pSelected);
  *pSelected = false;

  DWORD streamCount = 0;
  RETURN_IF_FAILED(m_spPresentationDescriptor->GetStreamDescriptorCount(&streamCount));
  for (unsigned int i = 0; i < streamCount; i++) {
    wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDescriptor;
    BOOL selected = FALSE;
    RETURN_IF_FAILED(m_spPresentationDescriptor->GetStreamDescriptorByIndex(i, &selected, &spStreamDescriptor));

    DWORD id = 0;
    RETURN_IF_FAILED(spStreamDescriptor->GetStreamIdentifier(&id));

    if (dwStreamId == id) {
      *pdwStreamIdx = i;
      *pSelected = !!selected;
      RETURN_IF_FAILED(spStreamDescriptor.copy_to(ppStreamDescriptor));
      return S_OK;
    }
  }
  return MF_E_NOT_FOUND;
}

IFACEMETHODIMP SimpleMediaSource::Stop() {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());

  if (m_sourceState != SourceState::Started) {
    return MF_E_INVALID_STATE_TRANSITION;
  }
  m_sourceState = SourceState::Stopped;

  wil::unique_prop_variant stopTime;
  RETURN_IF_FAILED(InitPropVariantFromInt64(MFGetSystemTime(), &stopTime));

  for (unsigned int i = 0; i < m_streamList.size(); i++) {
    RETURN_IF_FAILED(m_streamList[i]->Stop(true));
    RETURN_IF_FAILED(m_spPresentationDescriptor->DeselectStream(i));
  }

  RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, &stopTime));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::GetSourceAttributes(_COM_Outptr_ IMFAttributes** sourceAttributes) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_POINTER, sourceAttributes);
  *sourceAttributes = nullptr;
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  m_spAttributes.copy_to(sourceAttributes);
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::GetStreamAttributes(_In_ DWORD dwStreamIdentifier,
                                                       _COM_Outptr_ IMFAttributes** ppAttributes) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
  *ppAttributes = nullptr;
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());

  wil::com_ptr_nothrow<SimpleMediaStream> spStream;
  RETURN_IF_FAILED(_GetMediaStreamById(dwStreamIdentifier, &spStream));
  RETURN_IF_FAILED(spStream->m_spAttributes.copy_to(ppAttributes));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::SetD3DManager(_In_opt_ IUnknown*) { return E_NOTIMPL; }

IFACEMETHODIMP SimpleMediaSource::GetService(_In_ REFGUID, _In_ REFIID, _Out_ LPVOID* ppvObject) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_HR_IF_NULL(E_POINTER, ppvObject);
  *ppvObject = nullptr;
  return MF_E_UNSUPPORTED_SERVICE;
}

IFACEMETHODIMP SimpleMediaSource::SetDefaultAllocator(_In_ DWORD dwOutputStreamID, _In_ IUnknown* pAllocator) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());

  wil::com_ptr_nothrow<IMFVideoSampleAllocator> spAllocator;
  RETURN_IF_FAILED(pAllocator->QueryInterface(IID_PPV_ARGS(&spAllocator)));

  wil::com_ptr_nothrow<SimpleMediaStream> spStream;
  RETURN_IF_FAILED(_GetMediaStreamById(dwOutputStreamID, &spStream));
  RETURN_IF_FAILED(spStream->SetSampleAllocator(spAllocator.get()));
  return S_OK;
}

IFACEMETHODIMP SimpleMediaSource::GetAllocatorUsage(_In_ DWORD dwOutputStreamID, _Out_ DWORD* pdwInputStreamID,
                                                     _Out_ MFSampleAllocatorUsage* peUsage) {
  winrt::slim_lock_guard lock(m_Lock);
  RETURN_IF_FAILED(_CheckShutdownRequiresLock());
  RETURN_HR_IF_NULL(E_POINTER, pdwInputStreamID);
  RETURN_HR_IF_NULL(E_POINTER, peUsage);

  wil::com_ptr_nothrow<SimpleMediaStream> spStream;
  RETURN_IF_FAILED(_GetMediaStreamById(dwOutputStreamID, &spStream));
  *peUsage = spStream->SampleAlloactorUsage();
  *pdwInputStreamID = dwOutputStreamID;
  return S_OK;
}

HRESULT SimpleMediaSource::_CheckShutdownRequiresLock() {
  if (m_sourceState == SourceState::Shutdown) return MF_E_SHUTDOWN;
  if (m_spEventQueue == nullptr || m_streamList.get() == nullptr) return E_UNEXPECTED;
  return S_OK;
}

HRESULT SimpleMediaSource::_ValidatePresentationDescriptor(_In_ IMFPresentationDescriptor* pPD) {
  DWORD cStreams = 0;
  RETURN_HR_IF_NULL(E_INVALIDARG, pPD);
  RETURN_IF_FAILED(pPD->GetStreamDescriptorCount(&cStreams));
  if (cStreams != m_streamList.size()) return E_INVALIDARG;
  return S_OK;
}

HRESULT SimpleMediaSource::_CreateSourceAttributes(_In_opt_ IMFAttributes* pActivateAttributes) {
  RETURN_IF_FAILED(MFCreateAttributes(&m_spAttributes, 1));
  if (pActivateAttributes) {
    RETURN_IF_FAILED(pActivateAttributes->CopyAllItems(m_spAttributes.get()));
  }

  wil::com_ptr_nothrow<IMFSensorProfileCollection> profileCollection;
  wil::com_ptr_nothrow<IMFSensorProfile> profile;
  RETURN_IF_FAILED(MFCreateSensorProfileCollection(&profileCollection));

  const DWORD STREAM_ID = 0;
  RETURN_IF_FAILED(MFCreateSensorProfile(KSCAMERAPROFILE_Legacy, 0, nullptr, &profile));
  RETURN_IF_FAILED(profile->AddProfileFilter(STREAM_ID, L"((RES==;FRT<=30,1;SUT==))"));
  RETURN_IF_FAILED(profileCollection->AddProfile(profile.get()));

  RETURN_IF_FAILED(m_spAttributes->SetUnknown(MF_DEVICEMFT_SENSORPROFILE_COLLECTION, profileCollection.get()));

  return S_OK;
}

HRESULT SimpleMediaSource::_GetMediaStreamById(_In_ DWORD dwStreamId, _COM_Outptr_ SimpleMediaStream** ppStream) {
  RETURN_HR_IF_NULL(E_POINTER, ppStream);
  *ppStream = nullptr;
  for (unsigned int i = 0; i < m_streamList.size(); i++) {
    if (m_streamList[i]->Id() == dwStreamId) {
      *ppStream = m_streamList[i];
      (*ppStream)->AddRef();
      return S_OK;
    }
  }
  return MF_E_NOT_FOUND;
}
}  // namespace winrt::WindowsSample::implementation
