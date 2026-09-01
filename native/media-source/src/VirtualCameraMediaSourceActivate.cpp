#include "pch.h"

namespace winrt::WindowsSample::implementation {
IFACEMETHODIMP C4DPortalMediaSourceActivate::ActivateObject(REFIID riid, void** ppv) {
  RETURN_HR_IF_NULL(E_POINTER, ppv);
  *ppv = nullptr;

  m_spSimpleMediaSrc = winrt::make_self<winrt::WindowsSample::implementation::SimpleMediaSource>();
  RETURN_IF_FAILED(m_spSimpleMediaSrc->Initialize(this));
  RETURN_IF_FAILED(m_spSimpleMediaSrc->QueryInterface(riid, ppv));
  return S_OK;
}

IFACEMETHODIMP C4DPortalMediaSourceActivate::ShutdownObject() { return S_OK; }

IFACEMETHODIMP C4DPortalMediaSourceActivate::DetachObject() {
  m_spSimpleMediaSrc = nullptr;
  return S_OK;
}

IFACEMETHODIMP C4DPortalMediaSourceActivate::GetItem(_In_ REFGUID guidKey, _Inout_opt_ PROPVARIANT* pValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetItem(guidKey, pValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetItemType(_In_ REFGUID guidKey, _Out_ MF_ATTRIBUTE_TYPE* pType) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetItemType(guidKey, pType);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::CompareItem(_In_ REFGUID guidKey, _In_ REFPROPVARIANT Value, _Out_ BOOL* pbResult) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->CompareItem(guidKey, Value, pbResult);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::Compare(_In_ IMFAttributes* pTheirs, _In_ MF_ATTRIBUTES_MATCH_TYPE MatchType, _Out_ BOOL* pbResult) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->Compare(pTheirs, MatchType, pbResult);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetUINT32(_In_ REFGUID guidKey, _Out_ UINT32* punValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetUINT32(guidKey, punValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetUINT64(_In_ REFGUID guidKey, _Out_ UINT64* punValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetUINT64(guidKey, punValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetDouble(_In_ REFGUID guidKey, _Out_ double* pfValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetDouble(guidKey, pfValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetGUID(_In_ REFGUID guidKey, _Out_ GUID* pguidValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetGUID(guidKey, pguidValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetStringLength(_In_ REFGUID guidKey, _Out_ UINT32* pcchLength) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetStringLength(guidKey, pcchLength);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetString(_In_ REFGUID guidKey, _Out_writes_(cchBufSize) LPWSTR pwszValue, _In_ UINT32 cchBufSize, _Inout_opt_ UINT32* pcchLength) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetString(guidKey, pwszValue, cchBufSize, pcchLength);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetAllocatedString(_In_ REFGUID guidKey, _Out_writes_(*pcchLength + 1) LPWSTR* ppwszValue, _Inout_ UINT32* pcchLength) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetAllocatedString(guidKey, ppwszValue, pcchLength);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetBlobSize(_In_ REFGUID guidKey, _Out_ UINT32* pcbBlobSize) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetBlobSize(guidKey, pcbBlobSize);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetBlob(_In_ REFGUID guidKey, _Out_writes_(cbBufSize) UINT8* pBuf, UINT32 cbBufSize, _Inout_ UINT32* pcbBlobSize) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetAllocatedBlob(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8** ppBuf, __RPC__out UINT32* pcbSize) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetAllocatedBlob(guidKey, ppBuf, pcbSize);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppv) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetUnknown(guidKey, riid, ppv);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetItem(_In_ REFGUID guidKey, _In_ REFPROPVARIANT Value) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetItem(guidKey, Value);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::DeleteItem(_In_ REFGUID guidKey) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->DeleteItem(guidKey);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::DeleteAllItems() {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->DeleteAllItems();
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetUINT32(_In_ REFGUID guidKey, _In_ UINT32 unValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetUINT32(guidKey, unValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetUINT64(_In_ REFGUID guidKey, _In_ UINT64 unValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetUINT64(guidKey, unValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetDouble(_In_ REFGUID guidKey, _In_ double fValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetDouble(guidKey, fValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetGUID(_In_ REFGUID guidKey, _In_ REFGUID guidValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetGUID(guidKey, guidValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetString(_In_ REFGUID guidKey, _In_ LPCWSTR wszValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetString(guidKey, wszValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetBlob(_In_ REFGUID guidKey, _In_reads_(cbBufSize) const UINT8* pBuf, UINT32 cbBufSize) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetBlob(guidKey, pBuf, cbBufSize);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::SetUnknown(_In_ REFGUID guidKey, _In_ IUnknown* pUnknown) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->SetUnknown(guidKey, pUnknown);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::LockStore() {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->LockStore();
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::UnlockStore() {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->UnlockStore();
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetCount(_Out_ UINT32* pcItems) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetCount(pcItems);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::GetItemByIndex(UINT32 unIndex, _Out_ GUID* pguidKey, _Inout_ PROPVARIANT* pValue) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->GetItemByIndex(unIndex, pguidKey, pValue);
}
IFACEMETHODIMP C4DPortalMediaSourceActivate::CopyAllItems(_In_ IMFAttributes* pDest) {
  if (!m_spActivateAttributes) RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return m_spActivateAttributes->CopyAllItems(pDest);
}

HRESULT C4DPortalMediaSourceActivate::Initialize() {
  RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
  return S_OK;
}
}  // namespace winrt::WindowsSample::implementation
