#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/ActorPool.h"

CActorPool::CActorPool()
{
    for (PLAYERID actorId = 0; actorId < MAX_ACTORS; ++actorId)
    {
        m_pActors[actorId] = nullptr;
        m_bActorSlotState[actorId] = false;
        m_pGtaPed[actorId] = nullptr;
    }
}

CActorPool::~CActorPool()
{
    Reset();
}

bool CActorPool::IsValidActorId(PLAYERID actorId) const
{
    return actorId < MAX_ACTORS;
}

bool CActorPool::GetSlotState(PLAYERID actorId) const
{
    return IsValidActorId(actorId) && m_bActorSlotState[actorId] && m_pActors[actorId] != nullptr;
}

CActor* CActorPool::GetAt(PLAYERID actorId) const
{
    if (!GetSlotState(actorId)) return nullptr;
    return m_pActors[actorId];
}

bool CActorPool::New(NEW_ACTOR* newActor)
{
    if (!newActor || !IsValidActorId(newActor->ActorID)) return false;

    if (GetSlotState(newActor->ActorID))
        Delete(newActor->ActorID);

    if (!IsValidPedModel(newActor->iSkin))
        newActor->iSkin = 0;

    try
    {
        CActor* actor = new CActor(newActor->iSkin,
                                   newActor->vecPos.x,
                                   newActor->vecPos.y,
                                   newActor->vecPos.z,
                                   newActor->fAngle);
        if (!actor || !actor->m_pPed)
        {
            delete actor;
            return false;
        }

        actor->SetHealth(newActor->fHealth);
        actor->SetInvulnerable(newActor->bInvulnerable);

        m_pActors[newActor->ActorID] = actor;
        m_pGtaPed[newActor->ActorID] = actor->m_pPed;
        m_bActorSlotState[newActor->ActorID] = true;
        return true;
    }
    catch (...)
    {
        m_pActors[newActor->ActorID] = nullptr;
        m_pGtaPed[newActor->ActorID] = nullptr;
        m_bActorSlotState[newActor->ActorID] = false;
        return false;
    }
}

bool CActorPool::Delete(PLAYERID actorId)
{
    if (!IsValidActorId(actorId)) return false;

    CActor* actor = m_pActors[actorId];
    if (!actor)
    {
        m_bActorSlotState[actorId] = false;
        m_pGtaPed[actorId] = nullptr;
        return false;
    }

    m_bActorSlotState[actorId] = false;
    m_pGtaPed[actorId] = nullptr;
    m_pActors[actorId] = nullptr;
    delete actor;
    return true;
}

void CActorPool::Reset()
{
    for (PLAYERID actorId = 0; actorId < MAX_ACTORS; ++actorId)
        Delete(actorId);
}

PLAYERID CActorPool::FindIDFromGtaPtr(CPedGTA* ped) const
{
    if (!ped) return INVALID_PLAYER_ID;

    for (PLAYERID actorId = 0; actorId < MAX_ACTORS; ++actorId)
    {
        if (m_bActorSlotState[actorId] && m_pGtaPed[actorId] == ped)
            return actorId;
    }

    return INVALID_PLAYER_ID;
}
