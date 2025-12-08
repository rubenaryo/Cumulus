/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Avi Serebrenik
Date : 2025/11
Description : Useful functions for atmospheric rendering calculations
----------------------------------------------*/
#ifndef ATMOSPHEREUTILS_H
#define ATMOSPHEREUTILS_H

#include <Core/Camera.h>
#include <Utils/Utils.h>
#include <Core/MuonImgui.h>
#include <Core/CBufferStructs.h>
#include <DirectXMath.h>

namespace Muon
{

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
    float l = view_distance_meters / 1000.f;

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
    const float kFovY = 50.0f / 180.0f * XM_PI;

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

void UpdateAtmosphere(cbAtmosphere& constants,
    Camera& camera,
    AtmosphereInput& input)
{
    using namespace DirectX;

    // Calculate aspect ratio
    float aspect_ratio = static_cast<float>(input.viewport_width) / static_cast<float>(input.viewport_height);

    // FOV setup (50 degrees as in original)
    const float kFovY = 50.0f / 180.0f * XM_PI;

    input.view_zenith_angle_radians = camera.GetZenith();
    input.view_azimuth_angle_radians = camera.GetAzimuth();
    // Distance calculation is either based on simply height or distance from target
    XMVECTOR target = camera.GetTarget();
    XMVECTOR at = XMVectorSet(XMVectorGetX(target), XMVectorGetZ(target), XMVectorGetY(target), 0.0f);
    //float dist = max(XMVectorGetY(camera.GetPosition()), 0.f);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(at, camera.GetPosition())));
    // Create matrices
    // NOTE: Ideally we woudln't want to recalculate view from clip every time
    XMMATRIX view_from_clip = CreateViewFromClipMatrix(kFovY, aspect_ratio);
    XMMATRIX model_from_view = CreateModelFromViewMatrix(
        input.view_zenith_angle_radians,
        input.view_azimuth_angle_radians,
        dist * 50
    );

    // Store matrices (DirectX math uses row-major in memory, but these will actually still be like OpenGL column-major)
    XMStoreFloat4x4(&constants.view_from_clip, (view_from_clip));
    XMStoreFloat4x4(&constants.model_from_view, (model_from_view));

    // camera pos is grabbed from the calculation we already did for model matrix
    constants.camera_position = XMFLOAT3(constants.model_from_view._41, constants.model_from_view._42, constants.model_from_view._43);
    constants.isCamUp = input.view_zenith_angle_radians > XM_PIDIV2 - 0.1 ? 1 : 0;
    // Earth center (at origin in world space, but offset down in "length units")
    constants.earth_center = XMFLOAT3(0.0f, 0.0f, -6360.0f); // Earth radius in km
    // -0.989970, -0.141117, 0.006796 -> preset 2
    // -0.935575f, 0.230531f, 0.267499f -> preset 1
    if (input.isSunDynamic)
    {
        XMFLOAT3 axis = { -sqrt(3.f) * 0.5f, 0.0f, 0.5f};

        XMVECTOR rotation = XMQuaternionRotationAxis(XMVector3Normalize(XMLoadFloat3(&axis)), 0.005 * input.timeScale);
        // NOTE: z and y need to be flipped here because the atmosphere code expects Y up........
        XMFLOAT3 flipped = { input.sunDir.x, input.sunDir.z, input.sunDir.y };
        XMVECTOR currVec = XMLoadFloat3(&flipped);
        currVec = XMVector3Normalize(XMVector3Rotate(currVec, rotation));

        XMStoreFloat3(&constants.sun_direction, currVec);
    }
    else
    {
        // NOTE: this is explicit to switch Y and Z due to different coordinate systems at play
        XMFLOAT3 flipped = { input.sunDir.x, input.sunDir.z, input.sunDir.y };
        // Normalize sun direction
        XMVECTOR sun_dir = XMLoadFloat3(&flipped);
        sun_dir = XMVector3Normalize(sun_dir);

        XMStoreFloat3(&constants.sun_direction, sun_dir);
    }


    constants.sun_size = XMFLOAT2(0.004675f * input.sunSize, cos(0.004675f * input.sunSize));

    // Exposure and white point for tone mapping
    // NOTE: Maybe move some of this for post-process, so that clouds can use full color data
    constants.exposure = 10.0f * 1e-5; // Adjust as needed
    constants.white_point = XMFLOAT3(1.082414f, 0.967556f, 0.950030f);
}

struct LightingData
{
    // NOTE: ALL COLOR IS IN LINEAR
    float directExtinctionScale;
    float directStrength;
    DirectX::XMFLOAT3 sunColor;
    float sunIntensity;
    DirectX::XMFLOAT3 secondaryColor;
    float secondaryStrength;
    DirectX::XMFLOAT3 ambientColor;
    float ambientExtinction;
    float ambientStrength;
};

LightingData LerpLighting(const LightingData& from,
    const LightingData& to,
    float t)
{
    using namespace DirectX;
    LightingData out{};
    // start with all the pure floats
    out.directExtinctionScale = Lerp(from.directExtinctionScale, to.directExtinctionScale, t);
    out.directStrength = Lerp(from.directStrength, to.directStrength, t);
    out.sunIntensity = Lerp(from.sunIntensity, to.sunIntensity, t);
    out.secondaryStrength = Lerp(from.secondaryStrength, to.secondaryStrength, t);
    out.ambientExtinction = Lerp(from.ambientExtinction, to.ambientExtinction, t);
    out.ambientStrength = Lerp(from.ambientStrength, to.ambientStrength, t);
    // then we do the color, which we will assume comes and goes in linear space
    out.sunColor = OkLabToLinear(LerpOkLab(LinearToOkLab3(from.sunColor), LinearToOkLab3(to.sunColor), t));
    out.secondaryColor = OkLabToLinear(LerpOkLab(LinearToOkLab3(from.secondaryColor), LinearToOkLab3(to.secondaryColor), t));
    out.ambientColor = OkLabToLinear(LerpOkLab(LinearToOkLab3(from.ambientColor), LinearToOkLab3(to.ambientColor), t));

    return out;
}

void SetLightSettings(const LightingData& data, cbCloudLighting& settings)
{
    float directExtinctionScale;
    float directStrength;
    DirectX::XMFLOAT3 sunColor;
    float sunIntensity;
    DirectX::XMFLOAT3 secondaryColor;
    float secondaryStrength;
    DirectX::XMFLOAT3 ambientColor;
    float ambientExtinction;
    float ambientStrength;

    settings.directExtinctionScale = data.directExtinctionScale;
    settings.directStrength = data.directStrength;
    settings.sunColor = data.sunColor;
    settings.sunIntensity = data.sunIntensity;
    settings.secondaryColor = data.secondaryColor;
    settings.secondaryStrength = data.secondaryStrength;
    settings.ambientColor = data.ambientColor;
    settings.ambientExtinctionScale = data.ambientExtinction;
    settings.ambientStrength = data.ambientStrength;
}

void UpdateLightFromAtmosphere(cbAtmosphere& atmosphere, SceneSettings& settings)
{
    using namespace DirectX;
    XMVECTOR sunVec = XMLoadFloat3(&settings.atmosphere.sunDir);
    XMVECTOR up = { 0.f, 1.f, 0.f };
    float sun_height = XMVectorGetX(XMVector3Dot(sunVec, up));
    float nightVal = SmoothStep(0.20, -0.10, sun_height);

    // Setting colors
    // Daytime
    XMFLOAT3 noonCol (223.f / 255.f, 224.f / 255.f, 230.f / 255.f );
    XMFLOAT3 noonSecCol(153.f / 255.f, 181.f / 255.f, 207.f / 255.f);
    XMFLOAT3 noonAmbCol(210.f / 255.f, 222.f / 255.f, 255.f / 255.f);
    LightingData dayTimeData
    {
        0.107f,
        1.021f,
        SrgbToLinear3(noonCol),
        200.26f,
        SrgbToLinear3(noonSecCol),
        1.562f,
        SrgbToLinear3(noonAmbCol),
        0.002f,
        0.675f
    };
    // Nighttime
    XMFLOAT3 nightCol(48.f / 255.f, 55.f / 255.f, 73.f / 255.f);
    XMFLOAT3 nightSecCol(255.f / 255.f, 245.f / 255.f, 230.f / 255.f);
    XMFLOAT3 nightAmbCol(238.f / 255.f, 233.f / 255.f, 253.f / 255.f);
    LightingData nightLightData
    {
        0.07f,
        0.0f,
        SrgbToLinear3(nightCol),
        0.f,
        SrgbToLinear3(nightSecCol),
        0.067f,
        SrgbToLinear3(nightAmbCol),
        0.01f,
        0.209f
    };
    // SunRise
    XMFLOAT3 morningCol(255.f / 255.f, 135.f / 255.f, 0.f / 255.f);
    XMFLOAT3 morningCol2(161.f / 255.f, 194.f / 255.f, 224.f / 255.f);
    XMFLOAT3 morningCol3(216.f / 255.f, 224.f / 255.f, 247.f / 255.f);
    LightingData morningLightData
    {
        0.069f,
        0.272f,
        SrgbToLinear3(morningCol),
        189.39f,
        SrgbToLinear3(morningCol2),
        1.698f,
        SrgbToLinear3(morningCol3),
        0.013f,
        0.947f
    };

    // SunSet
    XMFLOAT3 eveningCol(173.f / 255.f, 72.f / 255.f, 25.f / 255.f);
    XMFLOAT3 eveningCol2(130.f / 255.f, 168.f / 255.f, 195.f / 255.f);
    XMFLOAT3 eveningCol3(49.f / 255.f, 83.f / 255.f, 106.f / 255.f);
    LightingData eveningLightData
    {
        0.023f,
        0.884f,
        SrgbToLinear3(eveningCol),
        69.27f,
        SrgbToLinear3(eveningCol2),
        0.094f,
        SrgbToLinear3(eveningCol3),
        0.007f,
        0.074f
    };

    // Lerping between different colors
    // For reference, nightVal = 1 is full night, 0 = full day.
    // If it is in between then we lerp.
    // Sunset and Sunrise (aka morning and evening) are different, and we check this with settings.isDay.
    // isDay is set to true when nightval = 0 and false when nightval = 1
    // Thus, if it is day and we are in between then we are in the evening state
    // if it is not day and we are in between then we are in the morning state
    LightingData lightData{};
    if (settings.isDay)
    {
        if (nightVal <= 0.5)
        {
            float t = nightVal * 2.f;
            lightData = LerpLighting(dayTimeData, eveningLightData, t);
        }
        else
        {
            float t = (nightVal - 0.5f) * 2.f;
            lightData = LerpLighting(eveningLightData, nightLightData, t);
            // switching to night -> NOTE: check if this 0.95 is good
            settings.isDay = t < 0.95;
        }
    }
    else
    {
        if (nightVal <= 0.5)
        {
            float t = nightVal * 2.f;
            lightData = LerpLighting(dayTimeData, morningLightData, t);
            settings.isDay = t < 0.05;
        }
        else
        {
            float t = (nightVal - 0.5f) * 2.f;
            lightData = LerpLighting(morningLightData, nightLightData, t);
        }
    }

    SetLightSettings(lightData, settings.lighting);

    // Setting sun direction
    if (nightVal < 0.95)
    {

        settings.lighting.dirSun = XMFLOAT3(-1.0 * settings.atmosphere.sunDir.x,
            1.0 * settings.atmosphere.sunDir.y,
            -1.0 * settings.atmosphere.sunDir.z);
    }
    // full nighttime
    else
    {
        settings.lighting.dirSun = XMFLOAT3(1.0 * settings.atmosphere.sunDir.x,
            -1.0 * settings.atmosphere.sunDir.y,
            1.0 * settings.atmosphere.sunDir.z);
    }

    settings.updateLighting = true;
}

}

#endif