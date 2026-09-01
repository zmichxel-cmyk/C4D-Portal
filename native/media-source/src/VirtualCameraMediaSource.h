#pragma once
#ifndef VIRTUALCAMERA_MEDIASOURCE_H
#define VIRTUALCAMERA_MEDIASOURCE_H

#include <initguid.h>

// Must match the sourceId GUID string passed to MFCreateVirtualCamera in
// native/virtual-camera/src/virtual_camera.cpp — that call is what tells
// Windows "when something opens this camera, CoCreateInstance this CLSID",
// and this is the CLSID this DLL registers itself under.
// {7B4F1A2E-9C3D-4E8A-B6F0-1A2B3C4D5E6F}
DEFINE_GUID(CLSID_C4DPortalMediaSource,
    0x7b4f1a2e, 0x9c3d, 0x4e8a, 0xb6, 0xf0, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f);

static LPCWSTR C4DPORTAL_MEDIASOURCE_CLSID = L"{7B4F1A2E-9C3D-4E8A-B6F0-1A2B3C4D5E6F}";

#endif
