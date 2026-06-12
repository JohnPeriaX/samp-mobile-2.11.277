#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/PickupPool.h"

#include <algorithm>
#include <cstring>

extern CGame* pGame;
extern CNetGame* pNetGame;

namespace
{
    constexpr uint32_t kPickupReportCooldownMs = 1000;
    constexpr uint16_t kInvalidPlayerId = 0xFFFF;
}

CPickupPool::CPickupPool()
{
    std::memset(m_Pickups, 0, sizeof(m_Pickups));
    std::memset(m_droppedWeapon, 0, sizeof(m_droppedWeapon));
    std::memset(m_bSlotState, 0, sizeof(m_bSlotState));

    m_iPickupCount = 0;
    m_activePickupIds.reserve(128);

    for (int i = 0; i < MAX_PICKUPS; ++i)
    {
        m_dwHandle[i] = 0;
        m_iGTAPickupID[i] = -1;
        m_dwPickupTimer[i] = 0;
        m_droppedWeapon[i].fromPlayer = kInvalidPlayerId;
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
    return IsValidPickupId(pickupId) && m_bSlotState[pickupId];
}

const PICKUP* CPickupPool::GetAt(int pickupId) const
{
    if (!GetSlotState(pickupId))
        return nullptr;

    return &m_Pickups[pickupId];
}

uint32_t CPickupPool::GetGTAPickupHandle(int pickupId) const
{
    if (!IsValidPickupId(pickupId))
        return 0;

    return m_dwHandle[pickupId];
}

int CPickupPool::GetGTAPickupId(int pickupId) const
{
    if (!IsValidPickupId(pickupId))
        return -1;

    return m_iGTAPickupID[pickupId];
}

void CPickupPool::AddActivePickup(int pickupId)
{
    if (!IsValidPickupId(pickupId))
        return;

    const auto id = static_cast<uint16_t>(pickupId);
    if (std::find(m_activePickupIds.begin(), m_activePickupIds.end(), id) == m_activePickupIds.end())
        m_activePickupIds.push_back(id);
}

void CPickupPool::RemoveActivePickup(int pickupId)
{
    if (!IsValidPickupId(pickupId))
        return;

    const auto id = static_cast<uint16_t>(pickupId);
    auto it = std::find(m_activePickupIds.begin(), m_activePickupIds.end(), id);
    if (it != m_activePickupIds.end())
        m_activePickupIds.erase(it);
}

void CPickupPool::RebuildActivePickupList()
{
    m_activePickupIds.clear();
    m_activePickupIds.reserve(128);

    for (int i = 0; i < MAX_PICKUPS; ++i)
    {
        if (m_bSlotState[i])
            m_activePickupIds.push_back(static_cast<uint16_t>(i));
    }
}

bool CPickupPool::CreateGTAPickup(int pickupId)
{
    if (!GetSlotState(pickupId) || !pGame)
        return false;

    if (m_dwHandle[pickupId] != 0)
        return true;

    int gtaPickupId = -1;
    const PICKUP& pickup = m_Pickups[pickupId];

    m_dwHandle[pickupId] = pGame->CreatePickup(
        pickup.iModel,
        pickup.iType,
        pickup.pos.x,
        pickup.pos.y,
        pickup.pos.z,
        &gtaPickupId
    );

    if (m_dwHandle[pickupId] == 0)
    {
        m_iGTAPickupID[pickupId] = -1;
        return false;
    }

    m_iGTAPickupID[pickupId] = gtaPickupId;
    m_dwPickupTimer[pickupId] = 0;
    return true;
}

void CPickupPool::DestroyGTAPickup(int pickupId)
{
    if (!IsValidPickupId(pickupId))
        return;

    if (m_dwHandle[pickupId] != 0)
        ScriptCommand(&destroy_pickup, m_dwHandle[pickupId]);

    m_dwHandle[pickupId] = 0;
    m_iGTAPickupID[pickupId] = -1;
    m_dwPickupTimer[pickupId] = 0;
}

void CPickupPool::ClearSlot(int pickupId)
{
    if (!IsValidPickupId(pickupId))
        return;

    std::memset(&m_Pickups[pickupId], 0, sizeof(PICKUP));
    m_droppedWeapon[pickupId].bDroppedWeapon = false;
    m_droppedWeapon[pickupId].fromPlayer = kInvalidPlayerId;
    m_bSlotState[pickupId] = false;
    DestroyGTAPickup(pickupId);
    RemoveActivePickup(pickupId);
}

void CPickupPool::New(PICKUP* pickup, int pickupId)
{
    if (!pickup || !IsValidPickupId(pickupId))
        return;

    if (!m_bSlotState[pickupId] && m_iPickupCount >= MAX_PICKUPS)
        return;

    if (m_bSlotState[pickupId])
        Destroy(pickupId);

    std::memcpy(&m_Pickups[pickupId], pickup, sizeof(PICKUP));
    m_droppedWeapon[pickupId].bDroppedWeapon = false;
    m_droppedWeapon[pickupId].fromPlayer = kInvalidPlayerId;
    m_bSlotState[pickupId] = true;
    m_dwPickupTimer[pickupId] = 0;
    ++m_iPickupCount;
    AddActivePickup(pickupId);

    // If the GTA world is not ready yet, keep the server-side slot active and
    // Process() will retry the native pickup creation later instead of losing it.
    CreateGTAPickup(pickupId);
}

void CPickupPool::Destroy(int pickupId)
{
    if (!IsValidPickupId(pickupId) || !m_bSlotState[pickupId])
        return;

    ClearSlot(pickupId);

    if (m_iPickupCount > 0)
        --m_iPickupCount;
}

void CPickupPool::Reset()
{
    auto activeIds = m_activePickupIds;
    for (uint16_t pickupId : activeIds)
        Destroy(pickupId);

    // Safety pass in case the active list was stale/corrupted by an old build.
    for (int i = 0; i < MAX_PICKUPS; ++i)
    {
        if (m_bSlotState[i] || m_dwHandle[i] != 0)
            ClearSlot(i);
    }

    m_iPickupCount = 0;
    m_activePickupIds.clear();
}

int CPickupPool::GetNumberFromGTAId(int gtaPickupId) const
{
    if (gtaPickupId < 0)
        return -1;

    for (uint16_t pickupId : m_activePickupIds)
    {
        if (m_bSlotState[pickupId] && m_dwHandle[pickupId] != 0 && m_iGTAPickupID[pickupId] == gtaPickupId)
            return pickupId;
    }

    return -1;
}

void CPickupPool::SetDroppedWeapon(int pickupId, uint16_t fromPlayer)
{
    if (!GetSlotState(pickupId))
        return;

    m_droppedWeapon[pickupId].bDroppedWeapon = true;
    m_droppedWeapon[pickupId].fromPlayer = fromPlayer;
}

void CPickupPool::ClearDroppedWeapon(int pickupId)
{
    if (!IsValidPickupId(pickupId))
        return;

    m_droppedWeapon[pickupId].bDroppedWeapon = false;
    m_droppedWeapon[pickupId].fromPlayer = kInvalidPlayerId;
}

bool CPickupPool::ShouldPollPickup(int pickupId) const
{
    if (!GetSlotState(pickupId) || m_dwHandle[pickupId] == 0)
        return false;

    // Mobile/PC refs poll dropped weapons and type 14 pickups. Other pickup
    // types are reported through CPickups::Update() -> PickedUp(gtaPickupId).
    return m_droppedWeapon[pickupId].bDroppedWeapon || m_Pickups[pickupId].iType == 14;
}

void CPickupPool::SendPickedUp(int pickupId, bool force)
{
    if (!pNetGame || !pNetGame->GetRakClient() || !GetSlotState(pickupId))
        return;

    const uint32_t now = GetTickCount();
    if (!force && m_dwPickupTimer[pickupId] != 0 && now - m_dwPickupTimer[pickupId] < kPickupReportCooldownMs)
        return;

    RakNet::BitStream bsPickup;
    if (m_droppedWeapon[pickupId].bDroppedWeapon)
        bsPickup.Write(m_droppedWeapon[pickupId].fromPlayer);
    else
        bsPickup.Write(pickupId);

    pNetGame->GetRakClient()->RPC(&RPC_PickedUpPickup, &bsPickup, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    m_dwPickupTimer[pickupId] = now;
}

void CPickupPool::PickedUp(int gtaPickupId)
{
    const int pickupId = GetNumberFromGTAId(gtaPickupId);
    if (pickupId == -1)
        return;

    if (m_droppedWeapon[pickupId].bDroppedWeapon)
        return;

    SendPickedUp(pickupId);
}

void CPickupPool::Process()
{
    if (!pNetGame || !pNetGame->GetRakClient())
        return;

    if (m_activePickupIds.size() != static_cast<size_t>(m_iPickupCount))
        RebuildActivePickupList();

    auto activeIds = m_activePickupIds;
    for (uint16_t pickupId : activeIds)
    {
        if (!GetSlotState(pickupId))
            continue;

        if (m_dwHandle[pickupId] == 0)
        {
            CreateGTAPickup(pickupId);
            continue;
        }

        if (ShouldPollPickup(pickupId) && ScriptCommand(&is_pickup_picked_up, m_dwHandle[pickupId]))
            SendPickedUp(pickupId);
    }
}
