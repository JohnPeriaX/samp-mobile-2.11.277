//
// Created by x1y2z on 08.11.2023.
//

#include "gta-reversed/game_sa/Pickups.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "vendor/patch/patch.h"
#include "gta-reversed/game_sa/World.h"
#include "gta-reversed/game_sa/Pickup.h"
#include "Camera.h"

extern CNetGame* pNetGame;
void CPickups::InjectHooks() {
}

void CPickups::Update() {
    static std::array<CPickup, MAX_NUM_PICKUPS>& aPickUps = *(std::array<CPickup, MAX_NUM_PICKUPS>*)(g_libGTASA + 0x983B00);

    auto start = 620 * (CTimer::GetFrameCounter() % 32) / 32;
    auto end   = 620 * (CTimer::GetFrameCounter() % 32 + 1) / 32;
    for (auto i = start; i < end; i++) {
        auto& pickup = aPickUps[i];
        if (pickup.m_nPickupType == PICKUP_NONE)
            continue;

        if (pickup.m_nFlags.bVisible = pickup.IsVisible()) {
            if (!pickup.m_nFlags.bDisabled && !pickup.m_pObject) {
                pickup.GiveUsAPickUpObject(&pickup.m_pObject, -1);

                if (auto& obj = pickup.m_pObject; obj) {
                    CWorld::Add(obj);
                }
            }
            if (pickup.Update()) {
                if (pNetGame && pNetGame->GetPickupPool()) {
                    pNetGame->GetPickupPool()->PickedUp(i);
                }
            }
        } else {
            pickup.GetRidOfObjects();
        }
    }
}
