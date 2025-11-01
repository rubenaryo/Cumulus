/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/2
Description : Transform class for game objects
----------------------------------------------*/
#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <Core/DXCore.h>
#include <DirectXMath.h>

namespace Muon
{
struct Transform
{
    explicit Transform();
    explicit Transform(DirectX::XMVECTOR pos, DirectX::XMVECTOR scale, DirectX::XMVECTOR rotQuat);

    // Returns World matrix from internal pos, scale, rot and stores it in mWorld
    DirectX::XMFLOAT4X4 Recompute();

    // Relative Transformers
    void Translate(float x, float y, float z);
    void Translate(DirectX::XMVECTOR translation);
    
    void Rotate(float pitch, float yaw, float roll);
    void Rotate(DirectX::XMVECTOR pitchYawRoll);
    
    void Scale(float x, float y, float z);
    void Scale(DirectX::XMVECTOR scales);

    // Absolute Transformers
    void SetTranslation(float x, float y, float z);
    void SetTranslation(DirectX::XMVECTOR translation);

    void SetRotation(float pitch, float yaw, float roll);
    void SetRotation(DirectX::XMVECTOR pitchYawRoll);

    void SetScale(float x, float y, float z);
    void SetScale(DirectX::XMVECTOR scales);

    DirectX::XMFLOAT4X4  mWorld;

private:
    // Transformation Info
    DirectX::XMVECTOR mPosition;
    DirectX::XMVECTOR mQuatRotation;
    DirectX::XMVECTOR mScale;
};
}
#endif