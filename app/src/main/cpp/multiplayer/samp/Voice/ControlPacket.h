#pragma once

#include "samp/Voice/include/util/Memory.h"

#pragma pack(push, 1)

struct ControlPacket
{
    uint16_t packet;
    uint16_t length;
    uint8_t data[];

    uint32_t GetFullSize();
};

#pragma pack(pop)

using ControlPacketContainer = Memory::ObjectContainer<ControlPacket>;
using ControlPacketContainerPtr = Memory::ObjectContainerPtr<ControlPacket>;
#define MakeControlPacketContainer MakeObjectContainer(ControlPacket)
