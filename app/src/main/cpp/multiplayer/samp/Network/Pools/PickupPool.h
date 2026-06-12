#pragma once

#include <cstdint>
#include <vector>

#include "gta-reversed/game_sa/Core/Vector.h"

#ifndef MAX_PICKUPS
#define MAX_PICKUPS 4096
#endif

#ifndef INVALID_PLAYER_ID
#define INVALID_PLAYER_ID 0xFFFF
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
    uint16_t fromPlayer;
};

class CPickupPool
{
public:
    CPickupPool();
    ~CPickupPool();

    bool GetSlotState(int pickupId) const;
    const PICKUP* GetAt(int pickupId) const;

    void New(PICKUP* pickup, int pickupId);
    void Destroy(int pickupId);
    void PickedUp(int gtaPickupId);
    void Process();
    void Reset();

    int GetNumberFromGTAId(int gtaPickupId) const;
    uint32_t GetGTAPickupHandle(int pickupId) const;
    int GetGTAPickupId(int pickupId) const;
    int GetActiveCount() const { return m_iPickupCount; }

    void SetDroppedWeapon(int pickupId, uint16_t fromPlayer);
    void ClearDroppedWeapon(int pickupId);

private:
    bool IsValidPickupId(int pickupId) const;
    bool CreateGTAPickup(int pickupId);
    void DestroyGTAPickup(int pickupId);
    void ClearSlot(int pickupId);

    void AddActivePickup(int pickupId);
    void RemoveActivePickup(int pickupId);
    void RebuildActivePickupList();

    bool ShouldPollPickup(int pickupId) const;
    void SendPickedUp(int pickupId, bool force = false);

private:
    int m_iPickupCount;
    bool m_bSlotState[MAX_PICKUPS];
    uint32_t m_dwHandle[MAX_PICKUPS];
    int m_iGTAPickupID[MAX_PICKUPS];
    uint32_t m_dwPickupTimer[MAX_PICKUPS];
    DROPPED_WEAPON m_droppedWeapon[MAX_PICKUPS];
    PICKUP m_Pickups[MAX_PICKUPS];
    std::vector<uint16_t> m_activePickupIds;
};
