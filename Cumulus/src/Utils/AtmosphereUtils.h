/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Useful functions for atmospheric rendering calculations
----------------------------------------------*/
#ifndef ATMOSPHEREUTILS_H
#define ATMOSPHEREUTILS_H

#include <Core/CBufferStructs.h>
#include <DirectXMath.h>

namespace Muon
{

void UpdateAtmosphereConstants(
    cbAtmosphere& constants,
    float view_distance_meters = 9000.0f,
    float view_zenith_angle_radians = 1.47f,
    float view_azimuth_angle_radians = -0.1f)
{
    using namespace DirectX;

    constexpr double kPi = 3.1415926;
    constexpr double kSunAngularRadius = 0.00935 / 2.0;
    constexpr double kSunSolidAngle = kPi * kSunAngularRadius * kSunAngularRadius;
    constexpr double kLengthUnitInMeters = 1000.0;

    // Camera position (in world space, in length units)
    float sin_zenith = sinf(view_zenith_angle_radians);
    float cos_zenith = cosf(view_zenith_angle_radians);
    float sin_azimuth = sinf(view_azimuth_angle_radians);
    float cos_azimuth = cosf(view_azimuth_angle_radians);

    constants.camera_position = XMFLOAT3(
        view_distance_meters * sin_zenith * cos_azimuth / kLengthUnitInMeters,
        view_distance_meters * sin_zenith * sin_azimuth / kLengthUnitInMeters,
        view_distance_meters * cos_zenith / kLengthUnitInMeters
    );

    // Earth center (at origin in world space, but offset down in "length units")
    constants.earth_center = XMFLOAT3(0.0f, 0.0f, -6360.0f); // Earth radius in km

    // Sun direction (example - adjust based on time of day)
    // This should point toward the sun. Example: sun at 30 degrees elevation
    float sun_elevation = 30.0f / 180.0f * static_cast<float>(kPi);
    constants.sun_direction = XMFLOAT3(
        cosf(sun_elevation),
        0.0f,
        sinf(sun_elevation)
    );

    // Normalize sun direction
    XMVECTOR sun_dir = XMLoadFloat3(&constants.sun_direction);
    sun_dir = XMVector3Normalize(sun_dir);
    XMStoreFloat3(&constants.sun_direction, sun_dir);

    // Sun size (angular radius and cosine of angular radius)
    constants.sun_size = XMFLOAT2(
        static_cast<float>(kSunAngularRadius),
        cosf(static_cast<float>(kSunAngularRadius))
    );

    // Exposure and white point for tone mapping
    constants.exposure = 10.0f; // Adjust as needed
    constants.white_point = XMFLOAT3(1.0f, 1.0f, 1.0f);

    // Display mode (0 = normal rendering, 1-3 = debug textures)
    constants.display_texture = 0;
    constants.scatter_slice = 0;
}
}

#endif