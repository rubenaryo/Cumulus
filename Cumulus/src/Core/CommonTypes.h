/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Common header to hold shared types
----------------------------------------------*/
#ifndef MUON_COMMONTYPES_H
#define MUON_COMMONTYPES_H

#include <stdint.h>

namespace Muon
{
    typedef uint32_t id_type;
    typedef id_type ShaderID;
    typedef id_type MeshID;
    typedef id_type TextureID;
    typedef id_type MaterialTypeID;

    static const id_type IDTYPE_MAX = UINT32_MAX;
    static const TextureID TEXTUREID_INVALID = IDTYPE_MAX;
}

#endif