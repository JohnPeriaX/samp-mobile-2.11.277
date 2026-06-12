#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/PickupPool.h"

extern CGame* pGame;
extern CNetGame* pNetGame;

namespace
{
    constexpr uint32_t kPickupReportCooldownMs = 1000;
}

CPickupPool::CPickupPool()
{
    memset(m_Pickups, 0, sizeof(m_Pickups));
    memset(m_droppedWeapon, 0, sizeof(m_droppedWeapon));

    m_iPickupCount = 0;

    for (int i = 0; i < MAX_PICKUPS; ++i)
    {
        m_dwHandle[i] = 0;
        m_iGTAPickupID[i] = -1;
        m_dwPickupTimer[i] = 0;
    }
}

CPickupPool::~CPickupPool()
{
    Reset();
}

bool CPickupPool::IsValidPickupId(int pickupId) const
{
    return pickupId >= 0 && pickupId < MAX_PICKUPS;
}

bool CPickupPool::GetSlotState(int pickupId) const
{
    return IsValidPickupId(pickupId) && m_dwHandle[pickupId] != 0;
}

uint32_t CPickupPool::GetGTAPickupHandle(int pickupId) const
{
    if (!IsValidPickupId(pickupId)) return 0;
    return m_dwHandle[pickupId];
}

int CPickupPool::GetGTAPickupId(int pickupId) const
{
    if (!IsValidPickupId(pickupId)) return -1;
    return m_iGTAPickupID[pickupId];
}

void CPickupPool::New(PICKUP* pickup, int pickupId)
{
    if (!pickup || !pGame || !IsValidPickupId(pickupId)) return;

    if (m_iPickupCount >= MAX_PICKUPS && !GetSlotState(pickupId)) return;

    if (GetSlotState(pickupId))
        Destroy(pickupId);

    memcpy(&m_Pickups[pickupId], pickup, sizeof(PICKUP));
    m_droppedWeapon[pickupId].bDroppedWeapon = false;
    m_droppedWeapon[pickupId].fromPlayer = 0xFFFF;

    int gtaPickupId = -1;
    m_dwHandle[pickupId] = pGame->CreatePickup(
        pickup->iModel,
        pickup->iType,
        pickup->pos.x,
        pickup->pos.y,
        pickup->pos.z,
        &gtaPickupId
    );

    if (m_dwHandle[pickupId] == 0)
    {
        memset(&m_Pickups[pickupId], 0, sizeof(PICKUP));
        m_iGTAPickupID[pickupId] = -1;
        return;
    }

    m_iGTAPickupID[pickupId] = gtaPickupId;
    m_dwPickupTimer[pickupId] = 0;
    ++m_iPickupCount;
}

void CPickupPool::Destroy(int pickupId)
{
    if (!IsValidPickupId(pickupId)) return;

    if (m_dwHandle[pickupId] != 0)
    {
        ScriptCommand(&destroy_pickup, m_dwHandle[pickupId]);
        m_dwHandle[pickupId] = 0;
        m_iGTAPickupID[pickupId] = -1;
        m_dwPickupTimer[pickupId] = 0;
        memset(&m_Pickups[pickupId], 0, sizeof(PICKUP));
        memset(&m_droppedWeapon[pickupId], 0, sizeof(DROPPED_WEAPON));

        if (m_iPickupCount > 0)
            --m_iPickupCount;
    }
}

void CPickupPool::Reset()
{
    for (int i = 0; i < MAX_PICKUPS; ++i)
        Destroy(i);

    m_iPickupCount = 0;
}

int CPickupPool::GetNumberFromGTAId(int gtaPickupId) const
{
    if (gtaPickupId < 0) return -1;

    for (int i = 0; i < MAX_PICKUPS; ++i)
    {
        if (m_dwHandle[i] != 0 && m_iGTAPickupID[i] == gtaPickupId)
            return i;
    }

    return -1;
}

void CPickupPool::SendPickedUp(int pickupId)
{
    if (!pNetGame || !pNetGame->GetRakClient() || !GetSlotState(pickupId)) return;

    if (m_droppedWeapon[pickupId].bDroppedWeapon)
        return;

    const uint32_t now = GetTickCount();
    if (m_dwPickupTimer[pickupId] != 0 && now - m_dwPickupTimer[pickupId] < kPickupReportCooldownMs)
        return;

    RakNet::BitStream bsPickup;
    bsPickup.Write(pickupId);
    pNetGame->GetRakClient()->RPC(&RPC_PickedUpPickup, &bsPickup, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, 0);

    m_dwPickupTimer[pickupId] = now;
}

void CPickupPool::PickedUp(int gtaPickupId)
{
    const int pickupId = GetNumberFromGTAId(gtaPickupId);
    if (pickupId == -1) return;

    SendPickedUp(pickupId);
}

void CPickupPool::Process()
{
    if (!pNetGame || !pNetGame->GetRakClient()) return;

    for (int i = 0; i < MAX_PICKUPS; ++i)
    {
        if (m_dwHandle[i] == 0)
            continue;

        if (ScriptCommand(&is_pickup_picked_up, m_dwHandle[i]))
        {
            if (m_droppedWeapon[i].bDroppedWeapon)
            {
                RakNet::BitStream bsPickup;
                bsPickup.Write(m_droppedWeapon[i].fromPlayer);
                pNetGame->GetRakClient()->RPC(&RPC_PickedUpPickup, &bsPickup, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, 0);
                m_dwPickupTimer[i] = GetTickCount();
            }
            else
            {
                SendPickedUp(i);
            }
        }
    }
}
