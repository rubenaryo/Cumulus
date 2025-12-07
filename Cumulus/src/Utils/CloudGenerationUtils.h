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
            x *= 512.f;
            y *= 64.f;
            z *= 512.f;
            s *= 200.f / num * scale;
            constants.seeds[i] = DirectX::XMFLOAT4(x, y, z, s);
        }
        constants.windOffset = DirectX::XMFLOAT2(0.f, 0.f);
    }

    void MoveClouds(
        cbCloudGenData& constants,
        const CloudInput& input)
    {
        using namespace DirectX;

        // First we update what the wind offset is
        XMFLOAT3 windInGoodCoord(input.windDir.x, 0.f, input.windDir.y);
        XMVECTOR windDir = XMLoadFloat3(&windInGoodCoord);
        windDir = XMVectorScale(windDir, input.windScale);

        XMFLOAT3 windPosFloat(constants.windOffset.x, 0.0, constants.windOffset.y);
        XMVECTOR windPosVec = XMLoadFloat3(&windPosFloat);
        windPosVec = XMVectorAdd(windPosVec, windDir);
        XMStoreFloat3(&windPosFloat, windPosVec);
        constants.windOffset = XMFLOAT2(windPosFloat.x, windPosFloat.z);

        // Next we check if a cloud is really out of bounds, and if so we regenerate it
        float halfEdgeLength = 256.f;
        float maxDist = sqrt(2.f * halfEdgeLength * halfEdgeLength);
        XMFLOAT3 gridCenterFloat(256.f, 256.f, 32.f);
        XMVECTOR gridCenter = XMLoadFloat3(&gridCenterFloat);

        for (int i = 0; i < constants.numSeeds; ++i)
        {
            XMFLOAT4 curr = XMFLOAT4(constants.seeds[i]);
            XMFLOAT3 currPos (curr.x, curr.y, curr.z);
            float scale = curr.w;

            XMVECTOR currVec = XMLoadFloat3(&currPos);
            // if we are out of bounds, regenerate at the flipped position
            XMVECTOR fromCenterToCloud = XMVectorSubtract(XMVectorAdd(currVec, windPosVec), gridCenter);
            if (XMVectorGetX(XMVector3Length(fromCenterToCloud)) > maxDist + scale)
            {
                XMVECTOR newPos = XMVectorScale(fromCenterToCloud, -0.5);
                XMVECTOR newCloud = XMVectorSetW(newPos, scale);
                XMStoreFloat4(&constants.seeds[i], newCloud);
            }
        }
    }

}
#endif