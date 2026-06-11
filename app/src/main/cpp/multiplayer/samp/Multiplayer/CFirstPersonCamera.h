//
// Created by vadim on 13.09.2025.
//

#pragma once
#include "gta-reversed/game_sa/Font.h"
#include <string>

class CFirstPersonCamera
{
    static bool m_bEnabled;
public:
    static void ProcessCameraOnFoot(uintptr_t pCam, CPlayerPed* pPed);
    static void ProcessCameraInVeh(uintptr_t pCam, CPlayerPed* pPed, CVehicle* pVeh);

    static void SetEnabled(bool bEnabled);
    static void Toggle();
    static bool IsEnabled();
};
