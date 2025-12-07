/*----------------------------------------------
Avi Serebrenik
Date : 2025/11
Description : Useful functions for cloud generation
----------------------------------------------*/
#ifndef CLOUDGENERATIONUTILS_H
#define CLOUDGENERATIONUTILS_H

#include <Core/Camera.h>
#include <Core/CBufferStructs.h>
#include <DirectXMath.h>

namespace Muon
{
    void GenerateCloudGenConstants(
        cbCloudGenData& constants,
        int num = 4,
        float scale = 1.0)
    {
        using namespace DirectX;
        constants.numSeeds = num;
        for (int i = 0; i < num; ++i)
        {
            float x = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) + 1.0f);
            float y = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) + 1.0f);
            float z = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) + 1.0f);
            float s = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) + 1.0f);
            //float x = cos(i * 12723.123);
            //float y = sin(i * 1284.789);
            //float z = sin(cos(i * 213.523) * 1924.23);
            x *= 4000.f;
            y *= 500.f;
            z *= 4000.f;
            s *= 200.f / num * scale;
            constants.seeds[i] = DirectX::XMFLOAT4(x, y, z, s);
        }
    }

}
#endif