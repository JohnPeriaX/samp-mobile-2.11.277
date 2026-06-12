#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/ActorPool.h"

#include <algorithm>
#include <cstring>

namespace
{
    constexpr uint32_t kActorRetryProcessMs = 250;

    void ResetAnimationState(ACTOR_ANIMATION_STATE& state)
    {
        state.bActive = false;
        state.szAnimName[0] = '\0';
        state.szAnimLib[0] = '\0';
        state.fDelta = 4.1f;
        state.bLoop = false;
        state.bLockX = false;
        state.bLockY = false;
        state.bFreeze = false;
        state.iTime = 0;
    }

    void CopyAnimString(char* dst, size_t dstSize, const char* src)
    {
        if (!dst || dstSize == 0) return;
        dst[0] = '\0';
        if (!src) return;
        std::strncpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = '\0';
    }
}

CActorPool::CActorPool()
{
    m_activeActorIds.reserve(64);
    m_dwLastProcessTick = 0;

    for (PLAYERID actorId = 0; actorId < MAX_ACTORS; ++actorId)
    {
        m_pActors[actorId] = nullptr;
        m_bActorSlotState[actorId] = false;
        m_pGtaPed[actorId] = nullptr;
        std::memset(&m_actorData[actorId], 0, sizeof(NEW_ACTOR));
        m_actorData[actorId].ActorID = actorId;
        ResetAnimationState(m_animationState[actorId]);
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
    return IsValidActorId(actorId) && m_bActorSlotState[actorId];
}

bool CActorPool::GetNativeSlotState(PLAYERID actorId) const
{
    return GetSlotState(actorId) && IsNativeActorValid(actorId);
}

CActor* CActorPool::GetAt(PLAYERID actorId) const
{
    if (!GetNativeSlotState(actorId)) return nullptr;
    return m_pActors[actorId];
}

void CActorPool::AddActiveActor(PLAYERID actorId)
{
    if (!IsValidActorId(actorId)) return;
    if (std::find(m_activeActorIds.begin(), m_activeActorIds.end(), actorId) == m_activeActorIds.end())
        m_activeActorIds.push_back(actorId);
}

void CActorPool::RemoveActiveActor(PLAYERID actorId)
{
    auto it = std::find(m_activeActorIds.begin(), m_activeActorIds.end(), actorId);
    if (it != m_activeActorIds.end())
        m_activeActorIds.erase(it);
}

bool CActorPool::IsNativeActorValid(PLAYERID actorId) const
{
    if (!IsValidActorId(actorId)) return false;

    CActor* actor = m_pActors[actorId];
    if (!actor || !actor->m_pPed) return false;
    if (!GamePool_Ped_GetAt(actor->m_dwGTAId)) return false;
    if (!IsValidGamePed(actor->m_pPed)) return false;

    return true;
}

bool CActorPool::CreateNativeActor(PLAYERID actorId)
{
    if (!GetSlotState(actorId)) return false;

    if (m_pActors[actorId])
    {
        delete m_pActors[actorId];
        m_pActors[actorId] = nullptr;
        m_pGtaPed[actorId] = nullptr;
    }

    NEW_ACTOR& actorInfo = m_actorData[actorId];
    if (!IsValidPedModel(actorInfo.iSkin))
        actorInfo.iSkin = 0;

    try
    {
        CActor* actor = new CActor(actorInfo.iSkin,
                                   actorInfo.vecPos.x,
                                   actorInfo.vecPos.y,
                                   actorInfo.vecPos.z,
                                   actorInfo.fAngle);
        if (!actor || !actor->m_pPed || !GamePool_Ped_GetAt(actor->m_dwGTAId))
        {
            delete actor;
            return false;
        }

        m_pActors[actorId] = actor;
        m_pGtaPed[actorId] = actor->m_pPed;
        ApplyCachedState(actorId);
        return true;
    }
    catch (...)
    {
        m_pActors[actorId] = nullptr;
        m_pGtaPed[actorId] = nullptr;
        return false;
    }
}

void CActorPool::ApplyCachedState(PLAYERID actorId)
{
    CActor* actor = m_pActors[actorId];
    if (!actor || !actor->m_pPed) return;

    NEW_ACTOR& actorInfo = m_actorData[actorId];
    actor->SetHealth(actorInfo.fHealth);
    actor->SetInvulnerable(actorInfo.bInvulnerable);
    actor->ForceTargetRotation(actorInfo.fAngle);
    actor->m_pPed->SetPosn(actorInfo.vecPos.x, actorInfo.vecPos.y, actorInfo.vecPos.z);

    ACTOR_ANIMATION_STATE& anim = m_animationState[actorId];
    if (anim.bActive)
    {
        actor->ApplyAnimation(anim.szAnimName, anim.szAnimLib, anim.fDelta,
                              anim.bLoop, anim.bLockX, anim.bLockY, anim.bFreeze, anim.iTime);
    }
}

bool CActorPool::New(NEW_ACTOR* newActor)
{
    if (!newActor || !IsValidActorId(newActor->ActorID)) return false;

    const PLAYERID actorId = newActor->ActorID;

    if (GetSlotState(actorId))
        Delete(actorId);

    m_actorData[actorId] = *newActor;
    m_actorData[actorId].ActorID = actorId;
    if (!IsValidPedModel(m_actorData[actorId].iSkin))
        m_actorData[actorId].iSkin = 0;

    m_bActorSlotState[actorId] = true;
    ResetAnimationState(m_animationState[actorId]);
    AddActiveActor(actorId);

    return CreateNativeActor(actorId);
}

void CActorPool::ClearSlot(PLAYERID actorId)
{
    if (!IsValidActorId(actorId)) return;

    if (m_pActors[actorId])
    {
        delete m_pActors[actorId];
        m_pActors[actorId] = nullptr;
    }

    m_bActorSlotState[actorId] = false;
    m_pGtaPed[actorId] = nullptr;
    std::memset(&m_actorData[actorId], 0, sizeof(NEW_ACTOR));
    m_actorData[actorId].ActorID = actorId;
    ResetAnimationState(m_animationState[actorId]);
}

bool CActorPool::Delete(PLAYERID actorId)
{
    if (!IsValidActorId(actorId)) return false;

    const bool hadSlot = m_bActorSlotState[actorId] || m_pActors[actorId] != nullptr;
    ClearSlot(actorId);
    RemoveActiveActor(actorId);
    return hadSlot;
}

void CActorPool::Reset()
{
    auto activeIds = m_activeActorIds;
    for (PLAYERID actorId : activeIds)
        Delete(actorId);

    for (PLAYERID actorId = 0; actorId < MAX_ACTORS; ++actorId)
    {
        if (m_bActorSlotState[actorId] || m_pActors[actorId])
            Delete(actorId);
    }

    m_activeActorIds.clear();
    m_dwLastProcessTick = 0;
}

void CActorPool::Process()
{
    const uint32_t now = GetTickCount();
    if (m_dwLastProcessTick != 0 && now - m_dwLastProcessTick < kActorRetryProcessMs)
        return;
    m_dwLastProcessTick = now;

    auto activeIds = m_activeActorIds;
    for (PLAYERID actorId : activeIds)
    {
        if (!GetSlotState(actorId))
        {
            RemoveActiveActor(actorId);
            continue;
        }

        if (!IsNativeActorValid(actorId))
        {
            m_pGtaPed[actorId] = nullptr;
            if (m_actorData[actorId].fHealth > 0.0f)
                CreateNativeActor(actorId);
        }
    }
}

PLAYERID CActorPool::FindIDFromGtaPtr(CPedGTA* ped) const
{
    if (!ped) return INVALID_PLAYER_ID;

    for (PLAYERID actorId : m_activeActorIds)
    {
        if (GetNativeSlotState(actorId) && m_pGtaPed[actorId] == ped)
            return actorId;
    }

    return INVALID_PLAYER_ID;
}

bool CActorPool::SetPosition(PLAYERID actorId, const CVector& vecPos)
{
    if (!GetSlotState(actorId)) return false;

    m_actorData[actorId].vecPos = vecPos;

    CActor* actor = GetAt(actorId);
    if (actor && actor->m_pPed)
    {
        actor->m_pPed->SetPosn(vecPos.x, vecPos.y, vecPos.z);
        return true;
    }

    return false;
}

bool CActorPool::SetFacingAngle(PLAYERID actorId, float fAngle)
{
    if (!GetSlotState(actorId)) return false;

    m_actorData[actorId].fAngle = fAngle;

    CActor* actor = GetAt(actorId);
    if (actor)
    {
        actor->ForceTargetRotation(fAngle);
        return true;
    }

    return false;
}

bool CActorPool::SetHealth(PLAYERID actorId, float fHealth)
{
    if (!GetSlotState(actorId)) return false;

    m_actorData[actorId].fHealth = fHealth;

    CActor* actor = GetAt(actorId);
    if (actor)
    {
        actor->SetHealth(fHealth);
        return true;
    }

    return false;
}

bool CActorPool::SetInvulnerable(PLAYERID actorId, bool bInvulnerable)
{
    if (!GetSlotState(actorId)) return false;

    m_actorData[actorId].bInvulnerable = bInvulnerable;

    CActor* actor = GetAt(actorId);
    if (actor)
    {
        actor->SetInvulnerable(bInvulnerable);
        return true;
    }

    return false;
}

bool CActorPool::ApplyAnimation(PLAYERID actorId, const char* szAnimName, const char* szAnimLib, float fDelta,
                                bool bLoop, bool bLockX, bool bLockY, bool bFreeze, int iTime)
{
    if (!GetSlotState(actorId) || !szAnimName || !szAnimLib) return false;

    ACTOR_ANIMATION_STATE& anim = m_animationState[actorId];
    anim.bActive = true;
    CopyAnimString(anim.szAnimName, sizeof(anim.szAnimName), szAnimName);
    CopyAnimString(anim.szAnimLib, sizeof(anim.szAnimLib), szAnimLib);
    anim.fDelta = fDelta;
    anim.bLoop = bLoop;
    anim.bLockX = bLockX;
    anim.bLockY = bLockY;
    anim.bFreeze = bFreeze;
    anim.iTime = iTime;

    CActor* actor = GetAt(actorId);
    if (actor)
    {
        actor->ApplyAnimation(anim.szAnimName, anim.szAnimLib, anim.fDelta,
                              anim.bLoop, anim.bLockX, anim.bLockY, anim.bFreeze, anim.iTime);
        return true;
    }

    return false;
}

bool CActorPool::ClearAnimation(PLAYERID actorId)
{
    if (!GetSlotState(actorId)) return false;

    ResetAnimationState(m_animationState[actorId]);

    CActor* actor = GetAt(actorId);
    if (actor)
    {
        actor->ClearAnimation();
        return true;
    }

    return false;
}
