//
// Created by x1y2z on 11.04.2023.
//

#include "VehicleModelInfo.h"
#include "vendor/patch/patch.h"
#include "gta-reversed/game_sa/RenderWare/rpworld.h"
#include "ModelInfo.h"
#include "gta-reversed/game_sa/VisibilityPlugins.h"

void CVehicleModelInfo::CVehicleModelInfo__SetClump(RpClump* clump)
{
    m_pVehicleStruct = new CVehicleStructure();
    CClumpModelInfo__SetClump(clump);
    SetAtomicRenderCallbacks();

    RwObjectNameIdAssocation* (&ms_vehicleDescs)[NUM_VEHICLE_MODEL_DESCS] = *(RwObjectNameIdAssocation*(*)[NUM_VEHICLE_MODEL_DESCS])(g_libGTASA + (VER_x32 ? 0x00687C7C : 0x8614C0));
    CClumpModelInfo::SetFrameIds(ms_vehicleDescs[m_nVehicleType]);
    SetRenderPipelines();
    PreprocessHierarchy();
    ReduceMaterialsInVehicle();
//    m_nCurrentPrimaryColor = 255;
//    m_nCurrentSecondaryColor = 255;
//    m_nCurrentTertiaryColor = 255;
//    m_nCurrentQuaternaryColor = 255;
//    SetCarCustomPlate();
}

void CVehicleModelInfo::CVehicleModelInfo__DeleteRwObject()
{
    delete m_pVehicleStruct;
    m_pVehicleStruct = nullptr;
    CClumpModelInfo__DeleteRwObject();
}

void CVehicleModelInfo::SetAtomicRenderCallbacks()
{
   CHook::CallFunction<void>("_ZN17CVehicleModelInfo24SetAtomicRenderCallbacksEv", this);
}

CVehicleModelInfo::CVehicleStructure::CVehicleStructure() : m_aUpgrades()
{
    for (auto& vecPos : m_avDummyPos)
        vecPos.Set(0.0F, 0.0F, 0.0F);

    for (auto& upgrade : m_aUpgrades)
        upgrade.m_nParentComponentId = -1;

    memset(m_apExtras, 0, sizeof(m_apExtras));
    m_nNumExtras = 0;
    m_nMaskComponentsDamagable = 0;
}

CVehicleModelInfo::CVehicleStructure::~CVehicleStructure()
{
    for (int32 i = 0; i < m_nNumExtras; ++i) {
        auto atomic = m_apExtras[i];
        auto frame = RpAtomicGetFrame(atomic);
        RpAtomicDestroy(atomic);
        RwFrameDestroy(frame);
    }
}

void CVehicleModelInfo::ReduceMaterialsInVehicle()
{
   CHook::CallFunction<void>("_ZN17CVehicleModelInfo24ReduceMaterialsInVehicleEv", this);
}


void CVehicleModelInfo::PreprocessHierarchy()
{
   CHook::CallFunction<void>("_ZN17CVehicleModelInfo19PreprocessHierarchyEv", this);
}

void CVehicleModelInfo::SetRenderPipelines()
{
    CHook::CallFunction<void>("_ZN17CVehicleModelInfo18SetRenderPipelinesEv", this);
}

void* CVehicleModelInfo::CVehicleStructure::operator new(size_t size)
{
    return m_pInfoPool->New();
}

void CVehicleModelInfo::CVehicleStructure::operator delete(void* data)
{
    m_pInfoPool->Delete(reinterpret_cast<CVehicleStructure*>(data));
}

void CVehicleModelInfo::InjectHooks() {
    //CHook::Write(g_libGTASA + (VER_x32 ? 0x679D00 : 0x851A18), &ms_pLightsTexture);
    //CHook::Write(g_libGTASA + (VER_x32 ? 0x67820C : 0x84E440), &ms_pLightsOnTexture);
    //CHook::Write(g_libGTASA + (VER_x32 ? 0x676CD4 : 0x84BA08), &ms_lightsOn);

    //CHook::InstallPLT(g_libGTASA + (VER_x32 ? 0x66E910 : 0x83DEE0), &SetEditableMaterials);
    //CHook::InstallPLT(g_libGTASA + (VER_x32 ? 0x675E30 : 0x849D10), &ResetEditableMaterials);

//    CVehicleStructure::m_pInfoPool = new CPool<CVehicleStructure>(CModelInfo::NUM_VEHICLE_MODEL_INFOS, "VehicleStruct");
//
//    CHook::Redirect("_ZN17CVehicleModelInfo8SetClumpEP7RpClump", &CVehicleModelInfo__SetClump_hook);
//    CHook::Redirect("_ZN17CVehicleModelInfo14DeleteRwObjectEv", &CVehicleModelInfo__DeleteRwObject_hook);
}
