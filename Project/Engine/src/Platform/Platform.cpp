#include "pch.h"
#include "Platform/IPlatform.h"

#ifdef ANDROID
#include "Platform/AndroidPlatform.h"
#else
#include "Platform/DesktopPlatform.h"
#endif

IPlatform* CreatePlatform() {
#ifdef ANDROID
    return new AndroidPlatform();
#else
    return new DesktopPlatform();
#endif
}