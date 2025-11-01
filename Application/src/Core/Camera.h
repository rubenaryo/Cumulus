/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/11
Description : Interface for Quaternion-Based Camera functionality 
----------------------------------------------*/
#ifndef CAMERA_H
#define CAMERA_H

#include "CBufferStructs.h"
#include "DXCore.h"
#include <Core/Buffers.h>

//namespace DirectX
//{
//    struct XMFLOAT3;
//}

namespace Input
{
    class GameInput;
}

namespace Muon
{

class Camera
{
    enum CameraMode
    {
        CM_ORTHOGRAPHIC,
        CM_PERSPECTIVE
    };

friend class Input::GameInput;

public:
    Camera();
    ~Camera();

public:
    void Init(DirectX::XMFLOAT3& pos, float aspectRatio, float nearPlane, float farPlane);
    void Destroy();

    void UpdateView();
    void UpdateProjection(float aspectRatio);

    void Bind(int32_t rootParamIndex, ID3D12GraphicsCommandList* pCommandList) const;

    DirectX::XMMATRIX   GetView()           const  { return mView;         }
    DirectX::XMMATRIX   GetProjection()     const  { return mProjection;   }
    float               GetSensitivity()    const  { return mSensitivity;  }
    
    void GetPosition3A(DirectX::XMFLOAT3A* out_pos) const;
    DirectX::XMVECTOR   GetPosition() const;

    void SetTarget(DirectX::XMVECTOR target);

private:
    // View and Projection Matrices
    DirectX::XMMATRIX   mView;
    DirectX::XMMATRIX   mProjection;
    DirectX::XMFLOAT4X4 mViewProjection;

    // Camera's local axis and position
    DirectX::XMVECTOR   mForward;
    DirectX::XMVECTOR   mRight;
    DirectX::XMVECTOR   mUp;
    DirectX::XMVECTOR   mPosition;
    DirectX::XMVECTOR   mTarget;

    // Position of near and far planes along forward axis
    float mNear;
    float mFar;

    // Look Sensitivity
    float mSensitivity;

    Muon::UploadBuffer mConstantBuffer;

    CameraMode mCameraMode;

private: // For GameInput only
    void MoveForward(float dist);
    void MoveRight(float dist);
    void MoveUp(float dist);
    void MoveAlongAxis(float dist, DirectX::XMVECTOR axis); // Assumes normalized axis
    void Rotate(DirectX::XMVECTOR quatRotation);
    
    void UpdateConstantBuffer();
};
}


#endif