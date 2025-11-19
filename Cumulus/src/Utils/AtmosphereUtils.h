/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Avi Serebrenik
Date : 2025/11
Description : Useful functions for atmospheric rendering calculations
----------------------------------------------*/
#ifndef ATMOSPHEREUTILS_H
#define ATMOSPHEREUTILS_H

#include <Core/Camera.h>
#include <Core/CBufferStructs.h>
#include <DirectXMath.h>

namespace Muon
{

// Constants from the original code
constexpr double kPi = 3.1415926;
constexpr double kSunAngularRadius = 0.00935 * 0.5f;
constexpr double kSunSolidAngle = kPi * kSunAngularRadius * kSunAngularRadius;
constexpr double kLengthUnitInMeters = 1000.0;

// Function to create the view_from_clip matrix (inverse projection)
DirectX::XMMATRIX CreateViewFromClipMatrix(float fovY_radians, float aspect_ratio)
{
    // In the original OpenGL code:
    // const float kTanFovY = tan(kFovY / 2.0);
    float tan_half_fov = tanf(fovY_radians * 0.5f);

    // Original OpenGL matrix (column-major):
    // [ tan_fov_y * aspect,  0,           0,   0  ]
    // [ 0,                   tan_fov_y,   0,   0  ]
    // [ 0,                   0,           0,   1  ]
    // [ 0,                   0,          -1,   1  ]

    // We need to use the same for our computations to match, even though DirectX is sometimes row-major:
    float m[16] = {
        tan_half_fov * aspect_ratio, 0.0f,         0.0f,  0.0f,
        0.0f,                        tan_half_fov, 0.0f,  0.0f,
        0.0f,                        0.0f,         0.0f, 1.0f,
        0.0f,                        0.0f,         -1.0f,  1.0f
    };

    return DirectX::XMMATRIX(m);
}

// Function to create the model_from_view matrix (inverse view matrix)
DirectX::XMMATRIX CreateModelFromViewMatrix(
    float view_zenith_angle_radians,
    float view_azimuth_angle_radians,
    float view_distance_meters  = 9000)
{
    using namespace DirectX;

    // Convert spherical coordinates to Cartesian for camera position
    // Using standard spherical coordinates:
    // x = r * sin(zenith) * cos(azimuth)
    // y = r * sin(zenith) * sin(azimuth)  
    // z = r * cos(zenith)

    float s_z = sinf(view_zenith_angle_radians);
    float c_z = cosf(view_zenith_angle_radians);
    float s_a = sinf(view_azimuth_angle_radians);
    float c_a = cosf(view_azimuth_angle_radians);
    float l = view_distance_meters / kLengthUnitInMeters;

    // This follows the method from Bruneton's code,
    // for a generic one, please look at older commits
    XMMATRIX model_from_view = XMMatrixSet(
        -s_a, c_a, 0.0, 0.0,
        -c_z * c_a, -c_z * s_a, s_z, 0.0,
        s_z * c_a, s_z * s_a, c_z, 0.0,
        s_z * c_a * l, s_z * s_a * l, c_z * l, 1.0);

    return model_from_view;
}

// Example usage function
void InitializeAtmosphereConstants(
    cbAtmosphere& constants,
    int viewport_width,
    int viewport_height,
    float view_distance_meters = 9000.0f,
    float view_zenith_angle_radians = 1.47f,
    float view_azimuth_angle_radians = -0.1f)
{
    using namespace DirectX;

    // Calculate aspect ratio
    float aspect_ratio = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    // FOV setup (50 degrees as in original)
    const float kFovY = 50.0f / 180.0f * static_cast<float>(kPi);

    // Create matrices
    XMMATRIX view_from_clip = CreateViewFromClipMatrix(kFovY, aspect_ratio);
    XMMATRIX model_from_view = CreateModelFromViewMatrix(
        view_zenith_angle_radians,
        view_azimuth_angle_radians,
        view_distance_meters
    );

    // Store matrices (DirectX math uses row-major in memory, but these will actually still be like OpenGL column-major)
    XMStoreFloat4x4(&constants.view_from_clip,  (view_from_clip));
    XMStoreFloat4x4(&constants.model_from_view, (model_from_view));

    //Muon::Printf("Model From View: \n");
    //Muon::Printf("%f, %f, %f, %f\n", constants.model_from_view._11, constants.model_from_view._12, constants.model_from_view._13, constants.model_from_view._14);
    //Muon::Printf("%f, %f, %f, %f\n", constants.model_from_view._21, constants.model_from_view._22, constants.model_from_view._23, constants.model_from_view._24);
    //Muon::Printf("%f, %f, %f, %f\n", constants.model_from_view._31, constants.model_from_view._32, constants.model_from_view._33, constants.model_from_view._34);
    //Muon::Printf("%f, %f, %f, %f\n", constants.model_from_view._41, constants.model_from_view._42, constants.model_from_view._43, constants.model_from_view._44);

    // camera pos is grabbed from the calculation we already did for model matrix
    constants.camera_position = XMFLOAT3(constants.model_from_view._41, constants.model_from_view._42, constants.model_from_view._43);
    constants.isCamUp = view_zenith_angle_radians > XM_PIDIV2 ? 1 : 0;
    // Earth center (at origin in world space, but offset down in "length units")
    constants.earth_center = XMFLOAT3(0.0f, 0.0f, -6360.0f); // Earth radius in km
    // -0.989970, -0.141117, 0.006796 -> preset 2
    // -0.935575f, 0.230531f, 0.267499f -> preset 1
    constants.sun_direction = XMFLOAT3(-0.935575f, 0.230531f, 0.267499f);

    // Normalize sun direction
    XMVECTOR sun_dir = XMLoadFloat3(&constants.sun_direction);
    sun_dir = XMVector3Normalize(sun_dir);
    XMStoreFloat3(&constants.sun_direction, sun_dir);

    constants.sun_size = XMFLOAT2(0.004675f, 0.999989f);

    // Exposure and white point for tone mapping
    // NOTE: Maybe move some of this for post-process, so that clouds can use full color data
    constants.exposure = 10.0f * 1e-5; // Adjust as needed
    constants.white_point = XMFLOAT3(1.082414f, 0.967556f, 0.950030f);
}

DirectX::XMFLOAT3 GetSunDirection(int time)
{
    using namespace DirectX;

    // mapping time to [0, 2PI]
    float t = static_cast<float>(time) / 2400.f * kPi * 2.f;

    // { -0.03931, 0.25845, -0.36024 }; I got this from crossing the first 2 presets, but it makes the sun never come up...
    XMVECTOR axis = { 1.0, 0.0, 0.0 };

    XMVECTOR rotation = XMQuaternionRotationAxis(axis, t);

    XMVECTOR midnight = { 0.0, 0.0, -1.0 };

    XMVECTOR currVec = XMVector3Rotate(midnight, rotation);

    XMFLOAT3 curr;
    XMStoreFloat3(&curr, currVec);

    return curr;
}

void UpdateAtmosphere(cbAtmosphere& constants,
    Camera& camera,
    bool isSunDynamic,
    int timeOfDay,
    float gameTime,
    float viewport_width = 1280,
    float viewport_height = 800,
    float view_zenith_angle_radians = 1.47f,
    float view_azimuth_angle_radians = -0.1f)
{
    using namespace DirectX;

    // Calculate aspect ratio
    float aspect_ratio = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    // FOV setup (50 degrees as in original)
    const float kFovY = 50.0f / 180.0f * static_cast<float>(kPi);

    view_zenith_angle_radians = camera.GetZenith();
    view_azimuth_angle_radians = camera.GetAzimuth();
    // Distance calculation is either based on simply height or distance from target
    //XMVECTOR target = camera.GetTarget();
    //XMVECTOR at = XMVectorSet(XMVectorGetX(target), XMVectorGetZ(target), XMVectorGetY(target), 0.0f);
    float dist = max(XMVectorGetY(camera.GetPosition()), 0.f);// XMVectorGetX(XMVector3Length(at));
    // Create matrices
    // NOTE: Ideally we woudln't want to recalculate view from clip every time
    XMMATRIX view_from_clip = CreateViewFromClipMatrix(kFovY, aspect_ratio);
    XMMATRIX model_from_view = CreateModelFromViewMatrix(
        view_zenith_angle_radians,
        view_azimuth_angle_radians,
        dist * 1000
    );

    // Store matrices (DirectX math uses row-major in memory, but these will actually still be like OpenGL column-major)
    XMStoreFloat4x4(&constants.view_from_clip, (view_from_clip));
    XMStoreFloat4x4(&constants.model_from_view, (model_from_view));

    // camera pos is grabbed from the calculation we already did for model matrix
    constants.camera_position = XMFLOAT3(constants.model_from_view._41, constants.model_from_view._42, constants.model_from_view._43);
    constants.isCamUp = view_zenith_angle_radians > XM_PIDIV2 ? 1 : 0;
    // Earth center (at origin in world space, but offset down in "length units")
    constants.earth_center = XMFLOAT3(0.0f, 0.0f, -6360.0f); // Earth radius in km
    // -0.989970, -0.141117, 0.006796 -> preset 2
    // -0.935575f, 0.230531f, 0.267499f -> preset 1
    // -0.03931, 0.25845, -0.36024 is the resulting axis, but it sucks aaaaahhhhh why doesn't the sun rise??
    //constants.sun_direction = XMFLOAT3(-0.935575f, 0.230531f, 0.267499f);
    if (isSunDynamic)
    {
        // current game time is an hour per second
        float mapped_time = fmodf(gameTime * 60.f, 2400.f);
        constants.sun_direction = GetSunDirection(mapped_time);
    }
    else
    {
        constants.sun_direction = GetSunDirection(timeOfDay);
    }
    // Normalize sun direction
    XMVECTOR sun_dir = XMLoadFloat3(&constants.sun_direction);
    sun_dir = XMVector3Normalize(sun_dir);
    XMStoreFloat3(&constants.sun_direction, sun_dir);

    constants.sun_size = XMFLOAT2(0.004675f, 0.999989f);

    // Exposure and white point for tone mapping
    // NOTE: Maybe move some of this for post-process, so that clouds can use full color data
    constants.exposure = 10.0f * 1e-5; // Adjust as needed
    constants.white_point = XMFLOAT3(1.082414f, 0.967556f, 0.950030f);
}

}

#endif