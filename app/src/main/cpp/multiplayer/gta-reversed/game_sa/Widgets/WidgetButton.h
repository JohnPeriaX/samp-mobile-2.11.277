//
// Created by x1y2zpenka on 04.09.2023.
//

#pragma once


#include "gta-reversed/game_sa/Widgets/WidgetGta.h"
#include "gta-reversed/game_sa/Sprite2D.h"

struct CWidgetButton : CWidgetGta
{
    uint32      m_nButtonFlags;
    float       m_fPulssateTime;
    CSprite2d   m_SpriteFill;
    float       m_fFillLevel;
    float       m_fTVTop;
    float       m_fTVBottom;
};
