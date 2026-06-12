#pragma once

#include <cstdint>
#include "gta-reversed/game_sa/Core/Vector.h"

class CActor;
class CPedGTA;

#ifndef MAX_ACTORS
#define MAX_ACTORS 1000
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

class CActorPool
{
public:
    CActorPool();
    ~CActorPool();

    bool GetSlotState(PLAYERID actorId) const;
    bool New(NEW_ACTOR* newActor);
    bool Delete(PLAYERID actorId);
    void Reset();

    CActor* GetAt(PLAYERID actorId) const;
    PLAYERID FindIDFromGtaPtr(CPedGTA* ped) const;

private:
    bool IsValidActorId(PLAYERID actorId) const;

private:
    CActor* m_pActors[MAX_ACTORS];
    bool m_bActorSlotState[MAX_ACTORS];
    CPedGTA* m_pGtaPed[MAX_ACTORS];
};
