#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/MenuPool.h"
#include "samp/GUI/imguirenderer.h"
#include "samp/GUI/uisettings.h"
#include "vendor/encoding/encoding.h"

#include <algorithm>
#include <cstring>
#include <string>

extern CNetGame* pNetGame;

namespace
{
    constexpr float kBaseWidth = 640.0f;
    constexpr float kBaseHeight = 448.0f;
    constexpr float kTitleHeight = 26.0f;
    constexpr float kHeaderHeight = 22.0f;
    constexpr float kRowHeight = 22.0f;
    constexpr float kPaddingX = 8.0f;
    constexpr float kBorder = 1.0f;
    constexpr uint32_t kMenuActionCooldownMs = 250;

    constexpr uint8_t TOUCH_POP = 1;
    constexpr uint8_t TOUCH_PUSH = 2;

    const char* kMenuItemNames[MAX_MENU_ITEMS][MAX_MENU_COLUMNS] =
    {
        {"SAMP000", "SAMP100"}, {"SAMP001", "SAMP101"}, {"SAMP002", "SAMP102"},
        {"SAMP003", "SAMP103"}, {"SAMP004", "SAMP104"}, {"SAMP005", "SAMP105"},
        {"SAMP006", "SAMP106"}, {"SAMP007", "SAMP107"}, {"SAMP008", "SAMP108"},
        {"SAMP009", "SAMP109"}, {"SAMP010", "SAMP110"}, {"SAMP011", "SAMP111"}
    };

    ImVec2 DisplaySize()
    {
        return ImGui::GetIO().DisplaySize;
    }
}

CMenu::CMenu(const char* title, float x, float y, uint8_t columns,
             float col1Width, float col2Width, const MENU_INT* interaction)
{
    std::memset(m_szTitle, 0, sizeof(m_szTitle));
    std::memset(m_szItems, 0, sizeof(m_szItems));
    std::memset(m_szHeader, 0, sizeof(m_szHeader));
    std::memset(m_byteColumnRowCount, 0, sizeof(m_byteColumnRowCount));
    std::memset(&m_menuInteraction, 0, sizeof(m_menuInteraction));

    CopyMenuText(m_szTitle, title, sizeof(m_szTitle));

    m_fXPos = x;
    m_fYPos = y;
    m_fCol1Width = std::max(1.0f, col1Width);
    m_fCol2Width = std::max(1.0f, col2Width);
    m_byteColumns = columns >= 2 ? 2 : 1;
    m_byteSelectedRow = INVALID_MENU_ROW;
    m_bVisible = false;

    if (interaction)
        std::memcpy(&m_menuInteraction, interaction, sizeof(MENU_INT));
}

void CMenu::CopyMenuText(char* dst, const char* src, size_t dstSize)
{
    if (!dst || dstSize == 0) return;

    std::memset(dst, 0, dstSize);
    if (!src) return;

    std::string utf = Encoding::cp2utf(src);
    if (utf.size() >= dstSize)
        utf.resize(dstSize - 1);

    std::strncpy(dst, utf.c_str(), dstSize - 1);
}

float CMenu::ScaleX(float value) const
{
    return value * DisplaySize().x / kBaseWidth;
}

float CMenu::ScaleY(float value) const
{
    return value * DisplaySize().y / kBaseHeight;
}

float CMenu::ColumnWidth(uint8_t column) const
{
    const float width = column == 0 ? m_fCol1Width : m_fCol2Width;
    return ScaleX(std::max(1.0f, width));
}

float CMenu::MenuWidth() const
{
    return ColumnWidth(0) + (m_byteColumns == 2 ? ColumnWidth(1) : 0.0f);
}

float CMenu::MenuHeight() const
{
    const uint8_t rows = std::max<uint8_t>(1, GetMaxRowCount());
    return ScaleY(kTitleHeight + kHeaderHeight + (kRowHeight * rows));
}

void CMenu::AddMenuItem(uint8_t column, uint8_t row, const char* text)
{
    if (column >= MAX_MENU_COLUMNS || row >= MAX_MENU_ITEMS) return;

    CopyMenuText(m_szItems[row][column], text, sizeof(m_szItems[row][column]));
    m_byteColumnRowCount[column] = std::max<uint8_t>(m_byteColumnRowCount[column], row + 1);
}

void CMenu::SetColumnTitle(uint8_t column, const char* text)
{
    if (column >= MAX_MENU_COLUMNS) return;
    CopyMenuText(m_szHeader[column], text, sizeof(m_szHeader[column]));
}

void CMenu::Show()
{
    m_bVisible = true;
    m_byteSelectedRow = FindFirstSelectableRow();
}

void CMenu::Hide()
{
    m_bVisible = false;
}

const char* CMenu::GetMenuItem(uint8_t column, uint8_t row) const
{
    if (column >= MAX_MENU_COLUMNS || row >= MAX_MENU_ITEMS) return "";
    return m_szItems[row][column];
}

const char* CMenu::GetMenuHeader(uint8_t column) const
{
    if (column >= MAX_MENU_COLUMNS) return "";
    return m_szHeader[column];
}

uint8_t CMenu::GetSelectedRow() const
{
    if (!CanSelect() || m_byteSelectedRow >= MAX_MENU_ITEMS || !IsRowEnabled(m_byteSelectedRow))
        return INVALID_MENU_ROW;
    return m_byteSelectedRow;
}

uint8_t CMenu::GetColumnRowCount(uint8_t column) const
{
    if (column >= MAX_MENU_COLUMNS) return 0;
    return m_byteColumnRowCount[column];
}

uint8_t CMenu::GetMaxRowCount() const
{
    uint8_t count = 0;
    for (uint8_t column = 0; column < m_byteColumns; ++column)
        count = std::max<uint8_t>(count, m_byteColumnRowCount[column]);

    if (count == 0)
    {
        for (uint8_t row = 0; row < MAX_MENU_ITEMS; ++row)
        {
            if (IsRowEnabled(row))
                count = row + 1;
        }
    }

    return std::min<uint8_t>(count == 0 ? 1 : count, MAX_MENU_ITEMS);
}

bool CMenu::IsRowEnabled(uint8_t row) const
{
    return row < MAX_MENU_ITEMS && m_menuInteraction.bRow[row] != 0;
}

bool CMenu::HasAnySelectableRow() const
{
    return FindFirstSelectableRow() != INVALID_MENU_ROW;
}

uint8_t CMenu::FindFirstSelectableRow() const
{
    if (!CanSelect()) return INVALID_MENU_ROW;

    for (uint8_t row = 0; row < MAX_MENU_ITEMS; ++row)
    {
        if (IsRowEnabled(row))
            return row;
    }

    return INVALID_MENU_ROW;
}

void CMenu::SetSelectedRow(uint8_t row)
{
    if (row >= MAX_MENU_ITEMS || !IsRowEnabled(row)) return;
    m_byteSelectedRow = row;
}

bool CMenu::HitTestMenu(float x, float y) const
{
    if (!m_bVisible) return false;

    const float menuX = ScaleX(m_fXPos);
    const float menuY = ScaleY(m_fYPos);
    const float width = MenuWidth();
    const float height = MenuHeight();

    return x >= menuX && x <= menuX + width && y >= menuY && y <= menuY + height;
}

int CMenu::HitTestRow(float x, float y) const
{
    if (!m_bVisible) return -1;

    const float menuX = ScaleX(m_fXPos);
    const float menuY = ScaleY(m_fYPos);
    const float width = MenuWidth();
    const float rowTop = menuY + ScaleY(kTitleHeight + kHeaderHeight);
    const float rowHeight = ScaleY(kRowHeight);
    const uint8_t rowCount = GetMaxRowCount();

    if (x < menuX || x > menuX + width || y < rowTop || y > rowTop + rowHeight * rowCount)
        return -1;

    const int row = static_cast<int>((y - rowTop) / rowHeight);
    return row >= 0 && row < rowCount && row < MAX_MENU_ITEMS ? row : -1;
}

void CMenu::Render(ImGuiRenderer* renderer)
{
    if (!renderer || !m_bVisible) return;

    const float menuX = ScaleX(m_fXPos);
    const float menuY = ScaleY(m_fYPos);
    const float width = MenuWidth();
    const float titleH = ScaleY(kTitleHeight);
    const float headerH = ScaleY(kHeaderHeight);
    const float rowH = ScaleY(kRowHeight);
    const float padX = ScaleX(kPaddingX);
    const float fontSize = UISettings::fontSize() * 0.45f;
    const float titleFontSize = UISettings::fontSize() * 0.52f;
    const uint8_t rowCount = GetMaxRowCount();

    const ImVec2 min(menuX, menuY);
    const ImVec2 max(menuX + width, menuY + MenuHeight());

    renderer->drawRect(min, max, ImColor(0, 0, 0, 160), true, 0.0f);
    renderer->drawRect(min, max, ImColor(255, 255, 255, 210), false, 0.0f, ScaleX(kBorder));

    renderer->drawRect(min, ImVec2(max.x, menuY + titleH), ImColor(0, 0, 0, 205), true, 0.0f);
    renderer->drawText(ImVec2(menuX + padX, menuY + ScaleY(3.0f)), ImColor(255, 255, 255, 255), m_szTitle, true, titleFontSize);

    float colX = menuX;
    for (uint8_t col = 0; col < m_byteColumns; ++col)
    {
        const float colW = ColumnWidth(col);
        renderer->drawRect(ImVec2(colX, menuY + titleH), ImVec2(colX + colW, menuY + titleH + headerH), ImColor(25, 25, 25, 210), true, 0.0f);
        renderer->drawText(ImVec2(colX + padX, menuY + titleH + ScaleY(2.0f)), ImColor(220, 220, 220, 255), m_szHeader[col], true, fontSize);
        colX += colW;
    }

    for (uint8_t row = 0; row < rowCount; ++row)
    {
        const float rowY = menuY + titleH + headerH + (rowH * row);
        const bool enabled = IsRowEnabled(row);
        const bool selected = enabled && row == m_byteSelectedRow;

        if (selected)
            renderer->drawRect(ImVec2(menuX, rowY), ImVec2(menuX + width, rowY + rowH), ImColor(230, 180, 40, 190), true, 0.0f);
        else if (row % 2 == 0)
            renderer->drawRect(ImVec2(menuX, rowY), ImVec2(menuX + width, rowY + rowH), ImColor(0, 0, 0, 75), true, 0.0f);

        colX = menuX;
        for (uint8_t col = 0; col < m_byteColumns; ++col)
        {
            const ImColor color = enabled ? ImColor(255, 255, 255, 255) : ImColor(170, 170, 170, 210);
            renderer->drawText(ImVec2(colX + padX, rowY + ScaleY(2.0f)), color, m_szItems[row][col], true, fontSize);
            colX += ColumnWidth(col);
        }
    }
}

bool CMenu::OnTouchEvent(int type, int x, int y, bool* shouldQuit, bool* shouldSelect)
{
    if (shouldQuit) *shouldQuit = false;
    if (shouldSelect) *shouldSelect = false;
    if (!m_bVisible) return false;

    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const int row = HitTestRow(fx, fy);

    if (row >= 0 && IsRowEnabled(static_cast<uint8_t>(row)))
    {
        SetSelectedRow(static_cast<uint8_t>(row));
        if (type == TOUCH_POP && shouldSelect)
            *shouldSelect = true;
        return true;
    }

    if (!HitTestMenu(fx, fy))
    {
        if (type == TOUCH_POP && shouldQuit)
            *shouldQuit = true;
        return type == TOUCH_PUSH || type == TOUCH_POP;
    }

    return true;
}

CMenuPool::CMenuPool()
{
    for (uint8_t menuId = 0; menuId < MAX_MENUS; ++menuId)
    {
        m_pMenus[menuId] = nullptr;
        m_bMenuSlotState[menuId] = false;
        m_bPendingShow[menuId] = false;
    }

    m_byteCurrentMenu = INVALID_MENU_ID;
    m_bExited = false;
    m_dwLastMenuActionTick = 0;
}

CMenuPool::~CMenuPool()
{
    Reset();
}

void CMenuPool::ClearSlot(uint8_t menuId)
{
    if (!IsMenuIdValid(menuId)) return;

    if (m_pMenus[menuId])
    {
        delete m_pMenus[menuId];
        m_pMenus[menuId] = nullptr;
    }

    m_bMenuSlotState[menuId] = false;
    m_bPendingShow[menuId] = false;

    if (m_byteCurrentMenu == menuId)
        m_byteCurrentMenu = INVALID_MENU_ID;
}

CMenu* CMenuPool::New(uint8_t menuId, const char* title, float x, float y, uint8_t columns,
                      float col1Width, float col2Width, const MENU_INT* interaction)
{
    if (!IsMenuIdValid(menuId)) return nullptr;

    const bool showAfterInit = m_bPendingShow[menuId];
    ClearSlot(menuId);

    CMenu* menu = new CMenu(title, x, y, columns, col1Width, col2Width, interaction);
    if (!menu) return nullptr;

    m_pMenus[menuId] = menu;
    m_bMenuSlotState[menuId] = true;

    if (showAfterInit)
        ShowMenu(menuId);

    return menu;
}

bool CMenuPool::Delete(uint8_t menuId)
{
    if (!IsMenuIdValid(menuId) || !m_bMenuSlotState[menuId] || !m_pMenus[menuId])
        return false;

    ClearSlot(menuId);
    return true;
}

CMenu* CMenuPool::GetAt(uint8_t menuId) const
{
    if (menuId >= MAX_MENUS) return nullptr;
    return m_pMenus[menuId];
}

bool CMenuPool::GetSlotState(uint8_t menuId) const
{
    return menuId < MAX_MENUS && m_bMenuSlotState[menuId] && m_pMenus[menuId] != nullptr;
}

void CMenuPool::ShowMenu(uint8_t menuId)
{
    if (!IsMenuIdValid(menuId)) return;

    if (!GetSlotState(menuId))
    {
        m_bPendingShow[menuId] = true;
        return;
    }

    if (m_byteCurrentMenu != INVALID_MENU_ID && GetSlotState(m_byteCurrentMenu) && m_byteCurrentMenu != menuId)
        m_pMenus[m_byteCurrentMenu]->Hide();

    m_pMenus[menuId]->Show();
    m_byteCurrentMenu = menuId;
    m_bExited = false;
}

void CMenuPool::HideMenu(uint8_t menuId)
{
    if (!IsMenuIdValid(menuId)) return;

    m_bPendingShow[menuId] = false;

    if (!GetSlotState(menuId))
    {
        if (m_byteCurrentMenu == menuId)
            m_byteCurrentMenu = INVALID_MENU_ID;
        return;
    }

    m_pMenus[menuId]->Hide();
    if (m_byteCurrentMenu == menuId)
        m_byteCurrentMenu = INVALID_MENU_ID;
}

void CMenuPool::Reset()
{
    for (uint8_t menuId = 0; menuId < MAX_MENUS; ++menuId)
        ClearSlot(menuId);

    m_byteCurrentMenu = INVALID_MENU_ID;
    m_bExited = false;
    m_dwLastMenuActionTick = 0;
}

void CMenuPool::Process()
{
    if (m_bExited && m_byteCurrentMenu != INVALID_MENU_ID)
    {
        HideMenu(m_byteCurrentMenu);
        m_bExited = false;
    }

    if (m_byteCurrentMenu != INVALID_MENU_ID && !GetSlotState(m_byteCurrentMenu))
        m_byteCurrentMenu = INVALID_MENU_ID;
}

void CMenuPool::Render(ImGuiRenderer* renderer)
{
    if (m_byteCurrentMenu == INVALID_MENU_ID || !GetSlotState(m_byteCurrentMenu)) return;
    m_pMenus[m_byteCurrentMenu]->Render(renderer);
}

bool CMenuPool::OnTouchEvent(int type, int x, int y)
{
    if (m_byteCurrentMenu == INVALID_MENU_ID || !GetSlotState(m_byteCurrentMenu)) return false;

    CMenu* menu = m_pMenus[m_byteCurrentMenu];
    bool shouldQuit = false;
    bool shouldSelect = false;
    const bool handled = menu->OnTouchEvent(type, x, y, &shouldQuit, &shouldSelect);

    if (shouldSelect && type == TOUCH_POP)
        SubmitCurrentSelection();
    else if (shouldQuit && type == TOUCH_POP)
        ExitCurrentMenu();

    return handled;
}

bool CMenuPool::SubmitCurrentSelection()
{
    if (m_byteCurrentMenu == INVALID_MENU_ID || !GetSlotState(m_byteCurrentMenu)) return false;

    CMenu* menu = m_pMenus[m_byteCurrentMenu];
    if (!menu || !menu->CanSelect()) return false;

    const uint8_t row = menu->GetSelectedRow();
    if (row == INVALID_MENU_ROW || !menu->IsRowEnabled(row)) return false;

    SendMenuSelect(row);
    m_bExited = true;
    return true;
}

bool CMenuPool::ExitCurrentMenu()
{
    if (m_byteCurrentMenu == INVALID_MENU_ID || !GetSlotState(m_byteCurrentMenu)) return false;

    SendMenuQuit();
    m_bExited = true;
    return true;
}

bool CMenuPool::CanSendMenuAction() const
{
    const uint32_t now = GetTickCount();
    return m_dwLastMenuActionTick == 0 || now - m_dwLastMenuActionTick >= kMenuActionCooldownMs;
}

void CMenuPool::MarkMenuActionSent()
{
    m_dwLastMenuActionTick = GetTickCount();
}

void CMenuPool::SendMenuSelect(uint8_t row)
{
    if (!pNetGame || !pNetGame->GetRakClient() || !CanSendMenuAction()) return;

    RakNet::BitStream bsSend;
    bsSend.Write(row);
    pNetGame->GetRakClient()->RPC(&RPC_MenuSelect, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    MarkMenuActionSent();
}

void CMenuPool::SendMenuQuit()
{
    if (!pNetGame || !pNetGame->GetRakClient() || !CanSendMenuAction()) return;

    pNetGame->GetRakClient()->RPC(&RPC_MenuQuit, nullptr, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    MarkMenuActionSent();
}

char* CMenuPool::GetTextPointer(const char* name)
{
    if (!name || m_byteCurrentMenu == INVALID_MENU_ID || !GetSlotState(m_byteCurrentMenu)) return nullptr;

    CMenu* menu = m_pMenus[m_byteCurrentMenu];
    if (!std::strcmp(name, "HED")) return const_cast<char*>(menu->GetMenuTitle());
    if (!std::strcmp(name, "RW1")) return const_cast<char*>(menu->GetMenuHeader(0));
    if (!std::strcmp(name, "RW2")) return const_cast<char*>(menu->GetMenuHeader(1));

    for (uint8_t row = 0; row < MAX_MENU_ITEMS; ++row)
    {
        if (!std::strcmp(name, kMenuItemNames[row][0] + 4)) return const_cast<char*>(menu->GetMenuItem(0, row));
        if (!std::strcmp(name, kMenuItemNames[row][1] + 4)) return const_cast<char*>(menu->GetMenuItem(1, row));
    }

    return nullptr;
}
