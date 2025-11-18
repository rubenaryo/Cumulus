/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/11
Description : Implementation of Camera Class
----------------------------------------------*/
#include "Camera.h"
#include <DirectXMath.h>
#include <Utils/Utils.h>
#include <algorithm>

namespace Muon 
{
using namespace DirectX;

Camera::Camera() :
    mNear(0.1f),
    mFar(100.0f),
    mSensitivity(1.0f),
    mForward(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f)),
    mUp(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f)),
    mRight(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f)),
    mCameraMode(CM_PERSPECTIVE)
{   
    mView = XMMatrixIdentity();
    mProjection = XMMatrixIdentity();
    mViewProjection = XMFLOAT4X4();
    mPosition = XMVectorZero();
    mTarget = XMVectorZero();
    mZenith = 1.57079632679f;   // camera starts with 0 height
    mAzimuth = 1.57079632679f;   // camera starts looking East -> positive X -> 90deg away from positive Z/North
}

Camera::~Camera()
{
}

void Camera::Init(DirectX::XMFLOAT3& pos, float aspectRatio, float nearPlane, float farPlane)
{
    mNear = nearPlane;
    mFar = farPlane;

    mPosition = XMLoadFloat3(&pos);
    mTarget = XMVectorZero();

    const float CAMERA_DIST = 1.0f; // TODO: Not sure where it's used

    mForward = XMVector3Normalize(XMVectorSubtract(mTarget, mPosition));

    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    mRight = XMVector3Normalize(XMVector3Cross(worldUp, mForward));
    mUp = XMVector3Cross(mForward, mRight);

    UpdateAzimuthZenith();

    mConstantBuffer.Create(L"CameraConstantBuffer", Muon::GetConstantBufferSize(sizeof(cbCamera)));

    UpdateView();
    UpdateProjection(aspectRatio);
}

void Camera::Destroy()
{
    mConstantBuffer.Destroy();
}

void Camera::UpdateView()
{
    // Create view matrix
    mView = XMMatrixLookToLH(
        mPosition,
        mForward,
        mUp);

    UpdateConstantBuffer();
}

void Camera::UpdateProjection(float aspectRatio)
{
    switch (mCameraMode)
    {
    case CM_ORTHOGRAPHIC:
    {
        mProjection = XMMatrixOrthographicLH(
            30,
            30,
            mNear,          // Near clip plane
            mFar);          // Far clip plane

        break;
    }
    case CM_PERSPECTIVE:
    {
        mProjection = XMMatrixPerspectiveFovLH(
            XM_PIDIV4,      // FOV
            aspectRatio,    // Screen Aspect ratio
            mNear,          // Near clip plane
            mFar);          // Far clip plane

        break;
    }
    default:
    {
        // Camera mode not set.
        assert(false);
        break;
    }
    }
}

void Camera::Bind(int32_t rootParamIndex, ID3D12GraphicsCommandList* pCommandList) const
{
    pCommandList->SetGraphicsRootConstantBufferView((UINT)rootParamIndex, mConstantBuffer.GetGPUVirtualAddress());
}

void Camera::GetPosition3A(XMFLOAT3A* out_pos) const
{
    XMStoreFloat3A(out_pos, mPosition);
}

XMVECTOR Camera::GetPosition() const
{
    return mPosition;
}

XMVECTOR Camera::GetTarget() const
{
    return mTarget;
}


float Camera::GetAzimuth() const
{
    return mAzimuth;
}
float Camera::GetZenith() const
{
    return mZenith;
}

void Camera::GetAxes(DirectX::XMVECTOR& forward, DirectX::XMVECTOR& right, DirectX::XMVECTOR& up) const
{
    forward = mForward;
    right = mRight;
    up = mUp;
}

void Camera::SetTarget(DirectX::XMVECTOR target)
{
    mTarget = target;
}

void Camera::MoveForward(float dist)
{
    MoveAlongAxis(dist, mForward);
}

void Camera::MoveRight(float dist)
{
    MoveAlongAxis(dist, mRight);
}

void Camera::MoveUp(float dist)
{
    MoveAlongAxis(dist, mUp);
}

void Camera::MoveAlongAxis(float dist, XMVECTOR axis)
{
    mPosition = XMVectorAdd(mPosition, XMVectorScale(axis, dist));
    mTarget = XMVectorAdd(mTarget, XMVectorScale(axis, dist));
    UpdateAzimuthZenith();
}

void Camera::Rotate(XMVECTOR quatRotation)
{

    // Calculate Forward vector
    mForward    = XMVector3Rotate(mForward, quatRotation);
    mUp         = XMVector3Rotate(mUp, quatRotation);
    mRight      = XMVector3Rotate(mRight, quatRotation);

    // Recalculate target, zenith, and azimuth
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(mPosition, mTarget)));
    mTarget = XMVectorAdd(XMVectorScale(mForward, dist), mPosition);
    UpdateAzimuthZenith();
}

void Camera::UpdateAzimuthZenith()
{
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR worldNorth = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    float dotZenith = XMVectorGetX(XMVector3Dot(XMVectorNegate(mForward), worldUp));
    dotZenith = std::clamp(dotZenith, -1.0f, 1.0f); // Prevent NaN
    mZenith = acosf(dotZenith);

    XMVECTOR flatProjectFwd = XMVectorSet(XMVectorGetX(mForward), 0.0f, XMVectorGetZ(mForward), 0.0f);
    // Check if we're looking straight up/down (degenerate case) -> keep prev value
    float flatLength = XMVectorGetX(XMVector3Length(flatProjectFwd));
    if (flatLength < 0.0001f) {
        return;
    }
    flatProjectFwd = XMVector3Normalize(flatProjectFwd);
    float dotAzimuth = XMVectorGetX(XMVector3Dot(flatProjectFwd, worldNorth));
    dotAzimuth = std::clamp(dotAzimuth, -1.0f, 1.0f); // Prevent NaN
    // Use cross product to determine quadrant
    XMVECTOR cross = XMVector3Cross(worldNorth, flatProjectFwd);
    float crossY = XMVectorGetY(cross);
    mAzimuth = acosf(dotAzimuth);
    // Adjust azimuth based on cross product to get full [0, 2PI] range
    if (crossY < 0.0f) {
        mAzimuth = XM_2PI - mAzimuth;
    }
}

void Camera::UpdateConstantBuffer()
{
    DirectX::XMMATRIX viewProj = XMMatrixMultiply(mView, mProjection);

    cbCamera cb;
    XMStoreFloat4x4(&cb.viewProj, viewProj);
    XMStoreFloat4x4(&cb.view, mView);
    XMStoreFloat4x4(&cb.proj, mProjection);
    XMStoreFloat4x4(&cb.invView, DirectX::XMMatrixInverse(nullptr, mView));
    XMStoreFloat4x4(&cb.invProj, DirectX::XMMatrixInverse(nullptr, mProjection));

    UINT8* pMappedMemory = mConstantBuffer.GetMappedPtr();
    if (!pMappedMemory)
    {
        Muon::Printf(L"Error: Failed to set camera constant buffer because it was unmapped!: %s\n", mConstantBuffer.GetName());
        return;
    }
    
    memcpy(pMappedMemory, &cb, mConstantBuffer.GetBufferSize());
}

}
