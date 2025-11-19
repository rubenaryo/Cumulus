// Atmosphere Constant Buffer
cbuffer cbAtmosphere : register(b0)
{
    float4x4 view_from_clip; // Inverse projection matrix
    float4x4 model_from_view; // Inverse view matrix
    
    float3 camera; // Camera position in world space (km units)
    float pad0;
    
    float3 earth_center; // Earth center position (km units)
    float pad1;
    
    float3 sun_direction; // Normalized sun direction
    float pad2;
    
    float2 sun_size; // x = angular radius, y = cos(angular radius)
    float exposure; // Tone mapping exposure
    int isCamUp;
    
    float3 white_point; // Tone mapping white point
    float pad3;
};