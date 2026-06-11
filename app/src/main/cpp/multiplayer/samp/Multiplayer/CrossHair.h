//
// Created by x1y2z on 10.11.2023.
//

#ifndef LIVERUSSIA_CROSSHAIR_H
#define LIVERUSSIA_CROSSHAIR_H


#include "gta-reversed/game_sa/RenderWare/rwcore.h"
#include "gta-reversed/game_sa/Sprite2D.h"

class CCrossHair {
public:
    static inline CSprite2d* pCircleTex;
    static inline bool m_UsedCrossHair{};

public:
    static void Init();
    static void Render();
};


#endif //LIVERUSSIA_CROSSHAIR_H