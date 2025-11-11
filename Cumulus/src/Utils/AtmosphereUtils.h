/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Useful functions for atmospheric rendering calculations
----------------------------------------------*/
#ifndef ATMOSPHEREUTILS_H
#define ATMOSPHEREUTILS_H

#include <Core/Camera.h>
#include <Core/CBufferStructs.h>

#include <Core/ResourceCodex.h>
#include <Core/DXCore.h>
#include <DirectXMath.h>

namespace Muon
{
class ComputePass;

// Function to create the view_from_clip matrix (inverse projection)
DirectX::XMMATRIX CreateViewFromClipMatrix(float fovY_radians, float aspect_ratio);

// Function to create the model_from_view matrix (inverse view matrix)
DirectX::XMMATRIX CreateModelFromViewMatrix(
    float view_distance_meters,
    float view_zenith_angle_radians,
    float view_azimuth_angle_radians);

// Example usage function
void UpdateAtmosphereConstants(
    cbAtmosphere& constants,
    int viewport_width,
    int viewport_height,
    float view_distance_meters = 9000.0f,
    float view_zenith_angle_radians = 1.47f,
    float view_azimuth_angle_radians = -0.1f);


bool PrecomputeTransmittance(ComputePass& transmittancePass);

}

#endif