/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/11
Description : Implementation of Camera Class
----------------------------------------------*/
#include "Camera.h"
#include <DirectXMath.h>

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

    const float CAMERA_DIST = 1.0f;

    mForward = XMVector3Normalize(XMVectorSubtract(mTarget, mPosition));

    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    mRight = XMVector3Normalize(XMVector3Cross(worldUp, mForward));
    mUp = XMVector3Cross(mForward, mRight);

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
}

void Camera::Rotate(XMVECTOR quatRotation)
{
    // Calculate Forward vector
    mForward    = XMVector3Rotate(mForward, quatRotation);
    mUp         = XMVector3Rotate(mUp, quatRotation);
    mRight      = XMVector3Rotate(mRight, quatRotation);
}

void Camera::UpdateConstantBuffer()
{
    DirectX::XMMATRIX viewProj = XMMatrixMultiply(mView, mProjection);
    cbCamera cb;
    XMStoreFloat4x4(&cb.viewProj, viewProj);
    XMStoreFloat4x4(&cb.view, mView);
    XMStoreFloat4x4(&cb.proj, mProjection);

    void* pMappedMemory = mConstantBuffer.Map();
    memcpy(pMappedMemory, &cb, mConstantBuffer.GetBufferSize());
    mConstantBuffer.Unmap(0, mConstantBuffer.GetBufferSize());
}

}
