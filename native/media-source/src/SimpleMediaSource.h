#pragma once
#ifndef SIMPLEMEDIASOURCE_H
#define SIMPLEMEDIASOURCE_H

namespace winrt::WindowsSample::implementation {
// forward declaration
struct SimpleMediaStream;

// Adapted from Microsoft's Windows-Camera VirtualCamera sample
// (VirtualCameraMediaSource/SimpleMediaSource.h): a single-stream video
// media source that Media Foundation's Frame Server loads (out-of-process,
// in the Frame Server service) when an app opens the "C4D Portal" camera.
// Trimmed: no IKsControl custom color property (C4D Portal doesn't need it),
// no AppInfo/PFN lookup (not running inside a packaged app).
struct SimpleMediaSource : winrt::implements<SimpleMediaSource, IMFMediaSourceEx, IMFGetService,
                                              IMFSampleAllocatorControl> {
  SimpleMediaSource() = default;

 private:
  enum class SourceState { Invalid, Stopped, Started, Shutdown };

 public:
  // IMFMediaEventGenerator (inherited by IMFMediaSource)
  IFACEMETHODIMP BeginGetEvent(_In_ IMFAsyncCallback* pCallback, _In_ IUnknown* punkState) override;
  IFACEMETHODIMP EndGetEvent(_In_ IMFAsyncResult* pResult, _Out_ IMFMediaEvent** ppEvent) override;
  IFACEMETHODIMP GetEvent(DWORD dwFlags, _Out_ IMFMediaEvent** ppEvent) override;
  IFACEMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus,
                             _In_ const PROPVARIANT* pvValue) override;

  // IMFMediaSource (inherited by IMFMediaSourceEx)
  IFACEMETHODIMP CreatePresentationDescriptor(_Out_ IMFPresentationDescriptor** ppPresentationDescriptor) override;
  IFACEMETHODIMP GetCharacteristics(_Out_ DWORD* pdwCharacteristics) override;
  IFACEMETHODIMP Pause() override;
  IFACEMETHODIMP Shutdown() override;
  IFACEMETHODIMP Start(_In_ IMFPresentationDescriptor* pPresentationDescriptor,
                        _In_ const GUID* pguidTimeFormat, _In_ const PROPVARIANT* pvarStartPosition) override;
  IFACEMETHODIMP Stop() override;

  // IMFMediaSourceEx
  IFACEMETHODIMP GetSourceAttributes(_COM_Outptr_ IMFAttributes** ppAttributes) override;
  IFACEMETHODIMP GetStreamAttributes(DWORD dwStreamIdentifier, _COM_Outptr_ IMFAttributes** ppAttributes) override;
  IFACEMETHODIMP SetD3DManager(_In_opt_ IUnknown* pManager) override;

  // IMFGetService
  IFACEMETHODIMP GetService(_In_ REFGUID guidService, _In_ REFIID riid, _Out_ LPVOID* ppvObject) override;

  // IMFSampleAllocatorControl
  IFACEMETHODIMP SetDefaultAllocator(_In_ DWORD dwOutputStreamID, _In_ IUnknown* pAllocator) override;
  IFACEMETHODIMP GetAllocatorUsage(_In_ DWORD dwOutputStreamID, _Out_ DWORD* pdwInputStreamID,
                                    _Out_ MFSampleAllocatorUsage* peUsage) override;

  // Non-interface functions
  HRESULT Initialize(_In_ IMFAttributes* pAttributes);

 private:
  HRESULT _CheckShutdownRequiresLock();
  HRESULT _ValidatePresentationDescriptor(_In_ IMFPresentationDescriptor* pPresentationDescriptor);
  HRESULT _CreateSourceAttributes(_In_opt_ IMFAttributes* pActivateAttributes);
  HRESULT _GetStreamDescriptorByStreamId(_In_ DWORD dwStreamId, _Out_ DWORD* pdwStreamIdx,
                                          _Out_ bool* pSelected, _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor);
  HRESULT _GetMediaStreamById(_In_ DWORD dwStreamId, _COM_Outptr_ SimpleMediaStream** ppMediaStream);

  winrt::slim_mutex m_Lock;
  SourceState m_sourceState{SourceState::Invalid};

  wil::com_ptr_nothrow<IMFMediaEventQueue> m_spEventQueue;
  wil::com_ptr_nothrow<IMFPresentationDescriptor> m_spPresentationDescriptor;
  wil::com_ptr_nothrow<IMFAttributes> m_spAttributes;
  wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<SimpleMediaStream>> m_streamList;

  const DWORD NUM_STREAMS = 1;
  bool m_initalized = false;
};
}  // namespace winrt::WindowsSample::implementation

#endif
