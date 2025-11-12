#pragma once
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <Core/CommonTypes.h>

namespace Muon
{
    struct sbConvexHull
    {
        uint32_t pointOffset;
        uint32_t pointCount;

        uint32_t faceOffset;
        uint32_t faceCount;
    };
}