#pragma once

#include <cstdint>
#include <vector>
#include "gta-reversed/game_sa/Core/Vector.h"

typedef unsigned short PLAYERID;

class CActor;
class CPedGTA;

#ifndef MAX_ACTORS
#define MAX_ACTORS 1000
#endif

#ifndef INVALID_ACTOR_ID
#define INVALID_ACTOR_ID 0xFFFF
#endif

#pragma pack(push, 1)
struct NEW_ACTOR
{
    PLAYERID ActorID;
    int iSkin;
    CVector vecPos;
    float fAngle;
    float fHealth;
    bool bInvulnerable;
};
#pragma pack(pop)

struct ACTOR_ANIMATION_STATE
{
    bool bActive;
    char szAnimName[64];
    char szAnimLib[64];
    float fDelta;
    bool bLoop;
    bool bLockX;
    bool bLockY;
    bool bFreeze;
    int iTime;
};

class CActorPool
{
public:
    CActorPool();
    ~CActorPool();

    bool IsValidActorId(PLAYERID actorId) const;
    bool GetSlotState(PLAYERID actorId) const;
    bool GetNativeSlotState(PLAYERID actorId) const;

    bool New(NEW_ACTOR* newActor);
    bool Delete(PLAYERID actorId);
    void Reset();
    void Process();

    CActor* GetAt(PLAYERID actorId) const;
    PLAYERID FindIDFromGtaPtr(CPedGTA* ped) const;

    bool SetPosition(PLAYERID actorId, const CVector& vecPos);
    bool SetFacingAngle(PLAYERID actorId, float fAngle);
    bool SetHealth(PLAYERID actorId, float fHealth);
    bool SetInvulnerable(PLAYERID actorId, bool bInvulnerable);
    bool ApplyAnimation(PLAYERID actorId, const char* szAnimName, const char* szAnimLib, float fDelta,
                        bool bLoop, bool bLockX, bool bLockY, bool bFreeze, int iTime);
    bool ClearAnimation(PLAYERID actorId);

private:
    void ClearSlot(PLAYERID actorId);
    void AddActiveActor(PLAYERID actorId);
    void RemoveActiveActor(PLAYERID actorId);
    bool CreateNativeActor(PLAYERID actorId);
    bool IsNativeActorValid(PLAYERID actorId) const;
    void ApplyCachedState(PLAYERID actorId);

private:
    CActor* m_pActors[MAX_ACTORS];
    bool m_bActorSlotState[MAX_ACTORS];
    CPedGTA* m_pGtaPed[MAX_ACTORS];
    NEW_ACTOR m_actorData[MAX_ACTORS];
    ACTOR_ANIMATION_STATE m_animationState[MAX_ACTORS];
    std::vector<PLAYERID> m_activeActorIds;
    uint32_t m_dwLastProcessTick;
};
