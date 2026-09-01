// dllmain.cpp — DLL entry point, COM class object/registration exports.
#include "pch.h"

HINSTANCE g_hInst;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      g_hInst = hModule;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}

bool __stdcall winrt_can_unload_now() noexcept {
  if (winrt::get_module_lock()) {
    return false;
  }
  winrt::clear_factory_cache();
  return true;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
  return winrt_can_unload_now() ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(GUID const& clsid, GUID const& iid, void** result) {
  try {
    *result = nullptr;

    if (clsid == __uuidof(winrt::WindowsSample::implementation::C4DPortalMediaSourceActivate)) {
      return winrt::make_self<C4DPortalMediaSourceActivateFactory>()->QueryInterface(iid, result);
    }
    return winrt::hresult_class_not_available().to_abi();
  } catch (...) {
    return winrt::to_hresult();
  }
}

namespace {
// Registers this DLL's CLSID under HKLM\Software\Classes\CLSID so
// MFCreateVirtualCamera's Start() (which CoCreateInstance's this CLSID from
// the Frame Server service) can find it. Writing under HKLM requires the
// caller (regsvr32, or our own installer) to run elevated.
HRESULT RegisterClsid() {
  wchar_t modulePath[MAX_PATH];
  if (GetModuleFileNameW(g_hInst, modulePath, MAX_PATH) == 0) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  wchar_t keyPath[256];
  swprintf_s(keyPath, L"Software\\Classes\\CLSID\\%s", C4DPORTAL_MEDIASOURCE_CLSID);

  HKEY clsidKey = nullptr;
  LSTATUS status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &clsidKey, nullptr);
  if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);

  const wchar_t* friendlyName = L"C4D Portal Media Source";
  RegSetValueExW(clsidKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(friendlyName),
                 static_cast<DWORD>((wcslen(friendlyName) + 1) * sizeof(wchar_t)));

  HKEY inprocKey = nullptr;
  status = RegCreateKeyExW(clsidKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &inprocKey, nullptr);
  if (status == ERROR_SUCCESS) {
    RegSetValueExW(inprocKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(modulePath),
                   static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));
    const wchar_t* threadingModel = L"Both";
    RegSetValueExW(inprocKey, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(threadingModel),
                   static_cast<DWORD>((wcslen(threadingModel) + 1) * sizeof(wchar_t)));
    RegCloseKey(inprocKey);
  }

  RegCloseKey(clsidKey);
  return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
}

HRESULT UnregisterClsid() {
  wchar_t keyPath[256];
  swprintf_s(keyPath, L"Software\\Classes\\CLSID\\%s", C4DPORTAL_MEDIASOURCE_CLSID);
  // RegDeleteTreeW removes the key and its InprocServer32 subkey together.
  LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath);
  return (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) ? S_OK : HRESULT_FROM_WIN32(status);
}
}  // namespace

extern "C" HRESULT __stdcall DllRegisterServer() { return RegisterClsid(); }
extern "C" HRESULT __stdcall DllUnregisterServer() { return UnregisterClsid(); }
