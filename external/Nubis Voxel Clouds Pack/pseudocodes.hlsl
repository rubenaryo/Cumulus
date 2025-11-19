//
// Function to up-rez a Dimensional Profile NVDF using noise Detail Type and Density Scale NVDF’s
//
// Init density samples structure
VoxelCloudDensitySamples density_samples;

// Get Modeling Data
float3 sample_coord = (inSamplePosition - cVoxelCloudBoundsMin) / (cVoxelCloudBoundsMax - cVoxelCloudBoundsMin);
VoxelCloudModelingData modeling_data = GetVoxelCloudModelingData(sample_coord, inMipLevel);

// Up-res density only if dimensional profile is nonzero (skip empty space work)
if (modeling_data.mDimensionalProfile > 0.0)
{
    density_samples.mProfile = modeling_data.mDimensionalProfile * modeling_data.mDensityScale;

    // NOTE: Adjust field names if different in your struct (e.g., mType / mDetailType)
    density_samples.mFull = GetUprezzedVoxelCloudDensity(
        inRaymarchInfo,
        inSamplePosition,
        modeling_data.mDimensionalProfile,
        modeling_data.mType,          // <- use your detail/type field name here
        modeling_data.mDensityScale,
        inMipLevel,
        inHFDetails
    );
}
else
{
    density_samples.mFull = 0.0;
    density_samples.mProfile = 0.0;
}


float GetUprezzedVoxelCloudDensity(
    CloudRenderingRaymarchInfo inRaymarchInfo,
    float3 inSamplePosition,
    float inDimensionalProfile,
    float inType,
    float inDensityScale,
    float inMipLevel,
    bool inHFDetails)
{
    // Apply wind offset
    inSamplePosition -= float3(cCloudWindOffset.x, cCloudWindOffset.y, 0.0) * voxel_cloud_animation_speed;

    // Sample noise
    float mipmap_level = GetVoxelCloudMipLevel(inRaymarchInfo, inMipLevel);
    float4 noise = Cloud3DNoiseTextureC.SampleLOD(Cloud3DNoiseSamplerC, inSamplePosition * 0.01, mipmap_level);

    // Define wispy noise
    float wispy_noise = lerp(noise.r, noise.g, inDimensionalProfile);

    // Define billowy noise
    float billowy_type_gradient = pow(inDimensionalProfile, 0.25);
    float billowy_noise = lerp(noise.b * 0.3, noise.a * 0.3, billowy_type_gradient);

    // Define Noise composite - blend to wispy as the density scale decreases.
    float noise_composite = lerp(wispy_noise, billowy_noise, inType);

    // Get the hf noise which is to be applied nearby - First, get the distance from the sample to camera and only do the work within a distance of 150 meters.
    if (inHFDetails)
    {
        // Get the hf noise by folding the highest frequency billowy noise.
        float hhf_noise = saturate(
            lerp(
                1.0 - pow(abs(abs(noise.g * 2.0 - 1.0) * 2.0 - 1.0), 4.0),
                pow(abs(abs(noise.a * 2.0 - 1.0) * 2.0 - 1.0), 2.0),
                inType));

        // Apply the HF noise near camera.
        float hhf_noise_distance_range_blender = ValueRemap(inRaymarchInfo.mDistance, 50.0, 150.0, 0.9, 1.0);
        noise_composite = lerp(hhf_noise, noise_composite, hhf_noise_distance_range_blender);
    }

    // Composite Noises and use as a Value Erosion
    float uprezzed_density = ValueErosion(inDimensionalProfile, noise_composite);

    // Modify User density scale
    float powered_density_scale = pow(saturate(inDensityScale), 4.0);

    // Apply User Density Scale Data to Result
    uprezzed_density *= powered_density_scale;

    // Sharpen result and lower Density close to camera to both add details and reduce undersampling noise
    uprezzed_density = pow(uprezzed_density, lerp(0.3, 0.6, max(EPSILON, powered_density_scale)));
    if (inHFDetails)
    {
        float distance_range_blender = GetFractionFromValue(inRaymarchInfo.mDistance, 50.0, 150.0);
        uprezzed_density = pow(uprezzed_density, lerp(0.5, 1.0, distance_range_blender)) * lerp(0.666, 1.0, distance_range_blender);
    }

    // Return result with softened edges
    return uprezzed_density;
}
