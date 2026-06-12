#pragma once

#include <cstdint>
#include "gta-reversed/game_sa/Core/Vector.h"

#ifndef MAX_PICKUPS
#define MAX_PICKUPS 4096
#endif

#pragma pack(push, 1)
struct PICKUP
{
    int iModel;
    int iType;
    CVector pos;
};
#pragma pack(pop)

struct DROPPED_WEAPON
{
    bool bDroppedWeapon;
    PLAYERID fromPlayer;
};

class CPickupPool
{
public:
    CPickupPool();
    ~CPickupPool();

    bool GetSlotState(int pickupId) const;
    void New(PICKUP* pickup, int pickupId);
    void Destroy(int pickupId);
    void PickedUp(int gtaPickupId);
    void Process();
    void Reset();

    int GetNumberFromGTAId(int gtaPickupId) const;
    uint32_t GetGTAPickupHandle(int pickupId) const;
    int GetGTAPickupId(int pickupId) const;

private:
    bool IsValidPickupId(int pickupId) const;
    void SendPickedUp(int pickupId);

private:
    int m_iPickupCount;
    uint32_t m_dwHandle[MAX_PICKUPS];
    int m_iGTAPickupID[MAX_PICKUPS];
    uint32_t m_dwPickupTimer[MAX_PICKUPS];
    DROPPED_WEAPON m_droppedWeapon[MAX_PICKUPS];
    PICKUP m_Pickups[MAX_PICKUPS];
};
