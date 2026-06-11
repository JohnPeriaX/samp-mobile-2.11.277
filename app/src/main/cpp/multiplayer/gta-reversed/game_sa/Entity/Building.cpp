
#include "gta-reversed/game_sa/common.h"
#include "gta-reversed/game_sa/Pools.h"
#include "Building.h"
#include "gta-reversed/game_sa/Models/ModelInfo.h"
#include "vendor/patch/patch.h"
#include "gta-reversed/game_sa/Enums/eEntityType.h"


int32 gBuildings;

void CBuilding::InjectHooks()
{
}

CBuilding::CBuilding() : CEntityGTA()
{
    m_nType = ENTITY_TYPE_BUILDING;
    m_bUsesCollision = true;
}

void* CBuilding::operator new(size_t size)
{
    return GetBuildingPool()->New();
}

void CBuilding::operator delete(void* data)
{
    GetBuildingPool()->Delete(static_cast<CBuilding*>(data));
}

void CBuilding::ReplaceWithNewModel(int32 newModelIndex)
{
    CHook::CallFunction<void>(g_libGTASA + 0x3AA5F0, this, newModelIndex);
}

bool IsBuildingPointerValid(CBuilding* building)
{
    if (!building)
        return false;

    return GetBuildingPool()->IsObjectValid(building);
}
