#pragma once
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <Core/CommonTypes.h>

namespace Muon
{
    struct sbConvexHull
    {
        uint32_t pointOffset;   // must match HLSL 'uint'
        uint32_t pointCount;

        uint32_t faceOffset;   // must match HLSL 'uint'
        uint32_t faceCount;
    };
}