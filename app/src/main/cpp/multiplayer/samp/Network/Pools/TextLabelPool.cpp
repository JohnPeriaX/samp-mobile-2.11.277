#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/TextLabelPool.h"
#include "samp/GUI/imguirenderer.h"
#include "samp/GUI/uisettings.h"
#include "samp/Multiplayer/AimStuff.h"
#include "gta-reversed/game_sa/World.h"
#include "vendor/encoding/encoding.h"

#include <algorithm>
#include <cstring>
#include <string>

extern CNetGame* pNetGame;
extern CGame* pGame;

namespace
{
    constexpr float kPlayerHeadOffsetZ = 0.23f;
    constexpr float kMinScreenZ = 1.0f;
    constexpr uint32_t kLosCheckIntervalMs = 150;
    constexpr uint32_t kAttachedLabelResyncIntervalMs = 1000;
    constexpr float kLosRecheckDistanceSq = 1.0f;
    constexpr size_t kMaxTextLabelLine = 2048;

    using CalcScreenCoorsFn = void (*)(CVector*, CVector*, float*, float*, bool, bool);

    // 2.11 libGame.so.lst:
    // _ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb = 0x5F449C
    constexpr uintptr_t kCSpriteCalcScreenCoors_211 = 0x5F449C;

    bool ProjectWorldToScreen(const CVector& worldPos, CVector& screenPos)
    {
        if (!g_libGTASA) return false;

        screenPos = CVector{0.0f, 0.0f, 0.0f};
        reinterpret_cast<CalcScreenCoorsFn>(g_libGTASA + kCSpriteCalcScreenCoors_211)(
            const_cast<CVector*>(&worldPos), &screenPos, nullptr, nullptr, false, false);

        return screenPos.z >= kMinScreenZ;
    }

    bool IsInlineColorCodeRaw(const char* text)
    {
        if (!text || text[0] != '{') return false;
        for (int i = 1; i <= 7; ++i)
        {
            if (text[i] == '\0') return false;
        }
        if (text[7] != '}') return false;

        for (int i = 1; i < 7; ++i)
        {
            const char c = text[i];
            const bool isHex = (c >= '0' && c <= '9') ||
                               (c >= 'A' && c <= 'F') ||
                               (c >= 'a' && c <= 'f');
            if (!isHex) return false;
        }

        return true;
    }

    float DistanceSquaredRaw(const CVector& a, const CVector& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    size_t CountTextLines(const char* text)
    {
        if (!text || text[0] == '\0') return 0;

        size_t lines = 1;
        for (const char* cur = text; *cur != '\0'; ++cur)
        {
            if (*cur == '\n')
                ++lines;
        }
        return lines;
    }

    void UpdateActiveColorFromRange(const char* begin, const char* end, char activeColor[9])
    {
        if (!begin || !end || !activeColor) return;

        const char* cur = begin;
        while (cur < end && *cur != '\0')
        {
            if (*cur == '{' && cur + 7 < end && cur[7] == '}' && IsInlineColorCodeRaw(cur))
            {
                std::memcpy(activeColor, cur, 8);
                activeColor[8] = '\0';
                cur += 8;
                continue;
            }
            ++cur;
        }
    }

    void BuildLineBuffer(const char* begin, const char* end, const char* activeColor,
                         char* out, size_t outSize)
    {
        if (!out || outSize == 0) return;

        size_t written = 0;
        out[0] = '\0';

        if (activeColor && activeColor[0] != '\0')
        {
            while (written + 1 < outSize && activeColor[written] != '\0')
            {
                out[written] = activeColor[written];
                ++written;
            }
        }

        const char* cur = begin;
        while (cur && cur < end && *cur != '\0' && written + 1 < outSize)
        {
            out[written++] = (*cur == '\t') ? ' ' : *cur;
            ++cur;
        }

        out[written] = '\0';
    }
}

C3DTextLabelPool::C3DTextLabelPool()
{
    for (uint16_t i = 0; i < MAX_TEXT_LABELS; ++i)
    {
        m_pTextLabels[i] = nullptr;
        m_bSlotState[i] = false;
    }

    m_activeLabelIds.reserve(128);
    m_dwLastAttachedResyncTick = 0;
}

C3DTextLabelPool::~C3DTextLabelPool()
{
    Reset();
}

TEXT_LABEL* C3DTextLabelPool::GetLabelById(uint16_t labelId) const
{
    if (labelId == INVALID_3D_TEXT_LABEL)
        return nullptr;

    if (labelId < MAX_TEXT_LABELS)
        return (m_bSlotState[labelId] && m_pTextLabels[labelId] != nullptr) ? m_pTextLabels[labelId] : nullptr;

    auto it = m_extraTextLabels.find(labelId);
    return it != m_extraTextLabels.end() ? it->second : nullptr;
}

bool C3DTextLabelPool::GetSlotState(uint16_t labelId) const
{
    return GetLabelById(labelId) != nullptr;
}

bool C3DTextLabelPool::IsInlineColorCode(const char* text)
{
    if (!text || text[0] != '{') return false;
    for (int i = 1; i <= 7; ++i)
    {
        if (text[i] == '\0') return false;
    }
    if (text[7] != '}') return false;

    for (int i = 1; i < 7; ++i)
    {
        const char c = text[i];
        const bool isHex = (c >= '0' && c <= '9') ||
                           (c >= 'A' && c <= 'F') ||
                           (c >= 'a' && c <= 'f');
        if (!isHex) return false;
    }

    return true;
}

void C3DTextLabelPool::FilterColors(char* text)
{
    if (!text) return;

    char* src = text;
    char* dst = text;

    while (*src != '\0')
    {
        if (IsInlineColorCode(src))
        {
            src += 8;
            continue;
        }

        *dst++ = (*src == '\t') ? ' ' : *src;
        ++src;
    }

    *dst = '\0';
}

void C3DTextLabelPool::CopyTextToLabel(TEXT_LABEL* label, const char* text)
{
    if (!label) return;

    std::string utfText = Encoding::cp2utf(text ? text : "");
    if (utfText.size() > 2048)
        utfText.resize(2048);

    memset(label->text, 0, sizeof(label->text));
    memset(label->textWithoutColors, 0, sizeof(label->textWithoutColors));

    strncpy(label->text, utfText.c_str(), sizeof(label->text) - 1);
    strncpy(label->textWithoutColors, utfText.c_str(), sizeof(label->textWithoutColors) - 1);
    FilterColors(label->textWithoutColors);
}

float C3DTextLabelPool::DistanceSquared(const CVector& a, const CVector& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

void C3DTextLabelPool::RemoveActiveLabel(uint16_t labelId)
{
    auto it = std::find(m_activeLabelIds.begin(), m_activeLabelIds.end(), labelId);
    if (it != m_activeLabelIds.end())
        m_activeLabelIds.erase(it);
}

void C3DTextLabelPool::CreateTextLabel(uint16_t labelId, const char* text, uint32_t color,
                                       const CVector& pos, float drawDistance, bool useLOS,
                                       uint16_t attachedToPlayerID, uint16_t attachedToVehicleID)
{
    if (labelId == INVALID_3D_TEXT_LABEL) return;

    Delete(labelId);

    TEXT_LABEL* label = new TEXT_LABEL;
    memset(label, 0, sizeof(TEXT_LABEL));

    CopyTextToLabel(label, text);

    label->color = color;
    label->pos = pos;
    label->drawDistance = drawDistance;
    label->useLineOfSight = useLOS;
    label->attachedToPlayerID = attachedToPlayerID;
    label->attachedToVehicleID = attachedToVehicleID;
    label->offsetCoords = (attachedToPlayerID != INVALID_PLAYER_ID || attachedToVehicleID != INVALID_VEHICLE_ID)
                          ? pos : CVector{0.0f, 0.0f, 0.0f};
    label->lastLosCheckTick = 0;
    label->lastLosCheckPos = CVector{0.0f, 0.0f, 0.0f};
    label->lastLosVisible = true;
    label->lastResolvedWorldPos = CVector{0.0f, 0.0f, 0.0f};
    label->lastResolvedTick = 0;
    label->hasLastResolvedWorldPos = false;

    if (labelId < MAX_TEXT_LABELS)
    {
        m_pTextLabels[labelId] = label;
        m_bSlotState[labelId] = true;
    }
    else
    {
        m_extraTextLabels[labelId] = label;
    }

    RemoveActiveLabel(labelId);
    m_activeLabelIds.push_back(labelId);
}

void C3DTextLabelPool::Delete(uint16_t labelId)
{
    if (labelId == INVALID_3D_TEXT_LABEL) return;

    bool removed = false;
    if (labelId < MAX_TEXT_LABELS)
    {
        if (m_pTextLabels[labelId])
        {
            delete m_pTextLabels[labelId];
            m_pTextLabels[labelId] = nullptr;
            removed = true;
        }

        if (m_bSlotState[labelId])
            removed = true;

        m_bSlotState[labelId] = false;
    }
    else
    {
        auto it = m_extraTextLabels.find(labelId);
        if (it != m_extraTextLabels.end())
        {
            delete it->second;
            m_extraTextLabels.erase(it);
            removed = true;
        }
    }

    if (removed)
        RemoveActiveLabel(labelId);
}

void C3DTextLabelPool::Update3DLabel(uint16_t labelId, uint32_t color, const char* text)
{
    TEXT_LABEL* label = GetLabelById(labelId);
    if (!label) return;
    label->color = color;
    CopyTextToLabel(label, text);
    label->lastLosCheckTick = 0;
    label->lastLosVisible = true;
}

void C3DTextLabelPool::Reset()
{
    for (uint16_t i = 0; i < MAX_TEXT_LABELS; ++i)
    {
        if (m_pTextLabels[i])
        {
            delete m_pTextLabels[i];
            m_pTextLabels[i] = nullptr;
        }
        m_bSlotState[i] = false;
    }

    for (auto& pair : m_extraTextLabels)
    {
        delete pair.second;
    }
    m_extraTextLabels.clear();

    m_activeLabelIds.clear();
    m_dwLastAttachedResyncTick = 0;
}

void C3DTextLabelPool::RebuildActiveLabels()
{
    m_activeLabelIds.clear();
    for (uint16_t i = 0; i < MAX_TEXT_LABELS; ++i)
    {
        if (m_bSlotState[i] && m_pTextLabels[i] != nullptr)
            m_activeLabelIds.push_back(i);
    }

    for (const auto& pair : m_extraTextLabels)
    {
        if (pair.second != nullptr)
            m_activeLabelIds.push_back(pair.first);
    }
}

void C3DTextLabelPool::ResyncAttachedLabels()
{
    const uint32_t now = GetTickCount();
    if (m_dwLastAttachedResyncTick != 0 && now - m_dwLastAttachedResyncTick < kAttachedLabelResyncIntervalMs)
        return;

    m_dwLastAttachedResyncTick = now;

    // Rebuild the active render list from slot state. This fixes cases where
    // the player/ped was inactive during stream-in, app resume, or reconnect and
    // the label stayed in the pool but fell out of the active render list.
    RebuildActiveLabels();

    for (uint16_t labelId : m_activeLabelIds)
    {
        TEXT_LABEL* label = GetLabelById(labelId);
        if (!label) continue;

        if (label->attachedToPlayerID != INVALID_PLAYER_ID || label->attachedToVehicleID != INVALID_VEHICLE_ID)
        {
            // Force LOS recheck after resume/stream-in so a stale false LOS cache
            // does not keep valid remote labels hidden until the owner toggles or dies.
            // Keep lastResolvedWorldPos so labels remain visible while a remote
            // player is AFK/minimized and the ped temporarily stops updating.
            label->lastLosCheckTick = 0;
            label->lastLosVisible = true;
        }
    }
}

bool C3DTextLabelPool::GetLabelWorldPosition(TEXT_LABEL* label, CVector& outPos) const
{
    if (!label || !pNetGame) return false;

    if (label->attachedToPlayerID != INVALID_PLAYER_ID)
    {
        CPlayerPool* playerPool = pNetGame->GetPlayerPool();
        if (!playerPool) return false;

        const uint16_t attachedPlayerId = label->attachedToPlayerID;
        CRemotePlayer* remotePlayer = playerPool->GetAt(attachedPlayerId);

        CPlayerPed* remotePed = remotePlayer ? remotePlayer->GetPlayerPed() : nullptr;
        const bool remotePedReady = remotePed && remotePed->m_pPed && remotePed->m_pPed->IsAdded();

        // PC SA-MP behavior: the owner should not see their own attached 3DText.
        // Hide only when this is truly the local player's own id. If a mobile
        // reconnect/spawn race temporarily reports the same id while a remote ped
        // exists in that slot, prefer the valid remote ped so other-player labels
        // do not disappear.
        const uint16_t localPlayerId = playerPool->GetLocalPlayerID();
        if (attachedPlayerId == localPlayerId && !remotePedReady)
        {
            CLocalPlayer* localPlayer = playerPool->GetLocalPlayer();
            CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
            const bool localPlayerReady = localPlayer && localPlayer->m_bIsActive &&
                                          localPed && localPed->m_pPed && localPed->m_pPed->IsAdded();
            if (localPlayerReady)
                return false;
        }

        // Do not require CRemotePlayer::IsActive() here. On mobile, a remote
        // player can keep a valid ped while their app is minimized/AFK or during
        // stream-in/resume, but IsActive()/state may temporarily drop. PC-like
        // attached labels should still follow the last valid ped instead of
        // vanishing until the owner toggles the protection mode or respawns.
        if (remotePedReady)
        {
            remotePed->GetBonePosition(8, &outPos);
            outPos += label->offsetCoords;
            outPos.z += kPlayerHeadOffsetZ;

            label->lastResolvedWorldPos = outPos;
            label->lastResolvedTick = GetTickCount();
            label->hasLastResolvedWorldPos = true;
            return true;
        }

        // If the player slot still exists but the ped is temporarily unavailable
        // (remote app minimized, reconnect/resume race, or stream state gap), keep
        // drawing at the last resolved position. Delete/stream-out RPCs still clear
        // the label normally, so this does not recreate removed labels.
        if (playerPool->GetSlotState(attachedPlayerId) && label->hasLastResolvedWorldPos)
        {
            outPos = label->lastResolvedWorldPos;
            return true;
        }

        return false;
    }

    if (label->attachedToVehicleID != INVALID_VEHICLE_ID)
    {
        CVehiclePool* vehiclePool = pNetGame->GetVehiclePool();
        if (!vehiclePool || !vehiclePool->GetSlotState(label->attachedToVehicleID))
        {
            if (label->hasLastResolvedWorldPos)
            {
                outPos = label->lastResolvedWorldPos;
                return true;
            }
            return false;
        }

        CVehicle* vehicle = vehiclePool->GetAt(label->attachedToVehicleID);
        if (!vehicle || !vehicle->m_pVehicle || !vehicle->m_pVehicle->IsAdded())
        {
            if (label->hasLastResolvedWorldPos)
            {
                outPos = label->lastResolvedWorldPos;
                return true;
            }
            return false;
        }

        outPos = vehicle->m_pVehicle->GetPosition();
        outPos += label->offsetCoords;

        label->lastResolvedWorldPos = outPos;
        label->lastResolvedTick = GetTickCount();
        label->hasLastResolvedWorldPos = true;
        return true;
    }

    outPos = label->pos;
    return true;
}

void C3DTextLabelPool::DrawLabel(ImGuiRenderer* renderer, TEXT_LABEL* label, const CVector& worldPos)
{
    if (!renderer || !label || !pGame || !pNetGame) return;

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    if (!playerPool || !playerPool->GetLocalPlayer() || !playerPool->GetLocalPlayer()->GetPlayerPed()) return;

    CPlayerPed* localPed = playerPool->GetLocalPlayer()->GetPlayerPed();
    if (!localPed || !localPed->m_pPed || !localPed->m_pPed->IsAdded()) return;

    const CVector localPos = localPed->m_pPed->GetPosition();
    if (label->drawDistance > 0.0f &&
        DistanceSquared(localPos, worldPos) > label->drawDistance * label->drawDistance)
    {
        return;
    }

    if (label->useLineOfSight)
    {
        const uint32_t now = GetTickCount();
        const bool shouldRefreshLOS = label->lastLosCheckTick == 0 ||
                                      now - label->lastLosCheckTick >= kLosCheckIntervalMs ||
                                      DistanceSquaredRaw(label->lastLosCheckPos, worldPos) > kLosRecheckDistanceSq;

        if (shouldRefreshLOS)
        {
            CAMERA_AIM* cam = GameGetInternalAim();
            if (!cam) return;

            const CVector cameraPos{cam->pos1x, cam->pos1y, cam->pos1z};
            label->lastLosVisible = CWorld::GetIsLineOfSightClear(cameraPos, worldPos, true, false, false, true, false, false, false);
            label->lastLosCheckTick = now;
            label->lastLosCheckPos = worldPos;
        }

        if (!label->lastLosVisible)
            return;
    }

    CVector screenPos;
    if (!ProjectWorldToScreen(worldPos, screenPos)) return;

    const float fontSize = UISettings::fontSize() / 2.0f;
    const size_t lineCount = CountTextLines(label->text);
    if (lineCount == 0) return;

    const float totalHeight = fontSize * static_cast<float>(lineCount);
    float y = screenPos.y - (totalHeight * 0.5f);

    char activeColor[9] = {0};
    char lineBuffer[kMaxTextLabelLine + 9 + 1];

    const char* lineStart = label->text;
    while (lineStart && *lineStart != '\0')
    {
        const char* lineEnd = lineStart;
        while (*lineEnd != '\0' && *lineEnd != '\n')
            ++lineEnd;

        BuildLineBuffer(lineStart, lineEnd, activeColor, lineBuffer, sizeof(lineBuffer));
        if (lineBuffer[0] != '\0')
        {
            const float width = renderer->calculateTextSizeInline(lineBuffer, nullptr, fontSize).x;
            ImVec2 drawPos(screenPos.x - (width * 0.5f), y);
            renderer->drawTextInline(drawPos, __builtin_bswap32(label->color), lineBuffer, nullptr, true, fontSize);
        }

        UpdateActiveColorFromRange(lineStart, lineEnd, activeColor);
        y += fontSize;

        if (*lineEnd == '\0')
            break;
        lineStart = lineEnd + 1;
    }
}

void C3DTextLabelPool::Render(ImGuiRenderer* renderer)
{
    if (!renderer || !pNetGame) return;

    ResyncAttachedLabels();
    if (m_activeLabelIds.empty()) return;

    for (uint16_t labelId : m_activeLabelIds)
    {
        if (!GetSlotState(labelId)) continue;

        TEXT_LABEL* label = GetLabelById(labelId);
        if (!label || label->text[0] == '\0') continue;

        CVector worldPos;
        if (!GetLabelWorldPosition(label, worldPos)) continue;

        DrawLabel(renderer, label, worldPos);
    }
}
