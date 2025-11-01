/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/2
Description : Transform class implementation
----------------------------------------------*/
#include "Transform.h"

namespace Muon {

using namespace DirectX;

Transform::Transform() :
    mPosition       (XMVectorZero()),
    mScale          (XMVectorReplicate(1.0f)),
    mQuatRotation   (XMQuaternionIdentity())
{
    XMStoreFloat4x4(&mWorld, DirectX::XMMatrixIdentity());
}

Transform::Transform(DirectX::XMVECTOR pos, DirectX::XMVECTOR scale, DirectX::XMVECTOR rotQuat) :
    mPosition(pos),
    mScale(scale),
    mQuatRotation(rotQuat)
{
    // Generate proper world matrix for given parameters
    this->Recompute();
}

DirectX::XMFLOAT4X4 Transform::Recompute()
{
    // Recompute the world matrix
    XMMATRIX tempWorld = XMMatrixAffineTransformation(mScale, XMVectorZero(), mQuatRotation, mPosition);
    XMStoreFloat4x4(&mWorld, tempWorld);

    // Return it.
    return mWorld;
}

void Transform::Translate(float x, float y, float z)
{
    this->Translate(XMVectorSet(x, y, z, 0));
}
void Transform::Translate(XMVECTOR translation)
{
    mPosition = XMVectorAdd(mPosition, translation);
}

void Transform::Rotate(float p, float y, float r)
{
    this->Rotate(XMVectorSet(p,y,r,0));
}

void Transform::Rotate(XMVECTOR quatRotation)
{
    mQuatRotation = XMQuaternionMultiply(mQuatRotation, XMQuaternionRotationRollPitchYawFromVector(quatRotation));
}

void Transform::Scale(float x, float y, float z)
{
    this->Scale(XMVectorSet(x, y, z, 0));
}

void Transform::Scale(XMVECTOR scales)
{
    mScale = XMVectorAdd(mScale, scales);
}

void Transform::SetTranslation(float x, float y, float z)
{
    SetTranslation(XMVectorSet(x, y, z, 0));
}

void Transform::SetTranslation(DirectX::XMVECTOR translation)
{
    mPosition = translation;
}

void Transform::SetRotation(float pitch, float yaw, float roll)
{
    this->SetRotation(XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
}

void Transform::SetRotation(DirectX::XMVECTOR quatRotation)
{
    mQuatRotation = quatRotation;
}

void Transform::SetScale(float x, float y, float z)
{
    SetScale(XMVectorSet(x, y, z, 0));
}

void Transform::SetScale(DirectX::XMVECTOR scales)
{
    mScale = scales;
}

}