/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Common header to hold shared types
----------------------------------------------*/
#ifndef MUON_COMMONTYPES_H
#define MUON_COMMONTYPES_H

#include <stdint.h>
#include <DirectXMath.h>

namespace Muon
{
    typedef uint32_t id_type;
    typedef id_type ResourceID;

    static const id_type IDTYPE_MAX = UINT32_MAX;
    static const ResourceID ResourceID_INVALID = IDTYPE_MAX;

    static const int32_t ROOTIDX_INVALID = -1;

    struct AABB
    {
        DirectX::XMFLOAT3A min;
        DirectX::XMFLOAT3A max;
    };

    struct HullFace
    {
        int indices[3];
        DirectX::XMFLOAT3A normal;
        float distance;
    };
}

#endif