#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "gta-reversed/game_sa/Core/Vector.h"

class ImGuiRenderer;

static constexpr uint16_t INVALID_3D_TEXT_LABEL = 0xFFFF;

#pragma pack(push, 1)
struct TEXT_LABEL
{
    char text[2048 + 1];
    char textWithoutColors[2048 + 1];
    uint32_t color;
    CVector pos;
    float drawDistance;
    bool useLineOfSight;
    uint16_t attachedToPlayerID;
    uint16_t attachedToVehicleID;
    CVector offsetCoords;
    uint32_t lastLosCheckTick;
    CVector lastLosCheckPos;
    bool lastLosVisible;

    // Last valid attached world position. This keeps already-created labels
    // visible through remote AFK/app-minimize/re-stream gaps while the player
    // slot still exists. It does not create labels that the server never sent.
    CVector lastResolvedWorldPos;
    uint32_t lastResolvedTick;
    bool hasLastResolvedWorldPos;
};
#pragma pack(pop)

class C3DTextLabelPool
{
public:
    C3DTextLabelPool();
    ~C3DTextLabelPool();

    bool GetSlotState(uint16_t labelId) const;
    bool HasLabel(uint16_t labelId) const { return GetSlotState(labelId); }

    void CreateTextLabel(uint16_t labelId, const char* text, uint32_t color,
                         const CVector& pos, float drawDistance, bool useLOS,
                         uint16_t attachedToPlayerID, uint16_t attachedToVehicleID);
    void Delete(uint16_t labelId);
    void Update3DLabel(uint16_t labelId, uint32_t color, const char* text);
    void Reset();
    void ResyncAttachedLabels();
    void Render(ImGuiRenderer* renderer);

private:
    TEXT_LABEL* GetLabelById(uint16_t labelId) const;
    bool GetLabelWorldPosition(TEXT_LABEL* label, CVector& outPos) const;
    void DrawLabel(ImGuiRenderer* renderer, TEXT_LABEL* label, const CVector& worldPos);

    static void FilterColors(char* text);
    static bool IsInlineColorCode(const char* text);
    static void CopyTextToLabel(TEXT_LABEL* label, const char* text);
    static float DistanceSquared(const CVector& a, const CVector& b);
    void RemoveActiveLabel(uint16_t labelId);
    void RebuildActiveLabels();

private:
    TEXT_LABEL* m_pTextLabels[MAX_TEXT_LABELS];
    bool m_bSlotState[MAX_TEXT_LABELS];

    // SA-MP/open.mp and streamer-based servers can send 16-bit 3DText ids
    // outside the legacy mobile MAX_TEXT_LABELS array. Keep those labels in
    // an overflow map so labels that already exist before this client joins are
    // not silently ignored.
    std::unordered_map<uint16_t, TEXT_LABEL*> m_extraTextLabels;
    std::vector<uint16_t> m_activeLabelIds;
    uint32_t m_dwLastAttachedResyncTick;
};
