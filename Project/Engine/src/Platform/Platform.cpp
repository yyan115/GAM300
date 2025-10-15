/* Start Header ************************************************************************/
/*!
\file       Platform.cpp
\author     Yan Yu
\date       Oct 8, 2025
\brief      Platform factory function that creates the appropriate platform instance
            (Android or Desktop) based on compile-time platform detection

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

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