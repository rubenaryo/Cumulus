// Atmosphere Shader - HLSL (DirectX 12)

/**
 * Copyright (c) 2017 Eric Bruneton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// This HLSL version was created by Avi Serebrenik and Ruben Young, Fall 2025
// It modifies Bruneton's original code by combining all, and only, the functions needed
// for atmospheric scattering after the precompute step.
// It assumes that Luminance is used with a combined scattering texture
// and that the precomputed textures are passed in through
// Texture2D transmittance_texture : register(t0);
// Texture2D irradiance_texture : register(t1);
// Texture3D scattering_texture : register(t2);


#include "AtmosphereBuffer.hlsli"

//---------------------------
// CONSTANTS AND DEFINITIONS
//---------------------------

static const int TRANSMITTANCE_TEXTURE_WIDTH = 256;
static const int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
static const int SCATTERING_TEXTURE_R_SIZE = 32;
static const int SCATTERING_TEXTURE_MU_SIZE = 128;
static const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
static const int SCATTERING_TEXTURE_NU_SIZE = 8;
static const int IRRADIANCE_TEXTURE_WIDTH = 64;
static const int IRRADIANCE_TEXTURE_HEIGHT = 16;

static const float m = 1.0;
static const float nm = 1.0;
static const float rad = 1.0;
static const float sr = 1.0;
static const float watt = 1.0;
static const float lm = 1.0;
static const float PI = 3.14159265358979323846;
static const float km = 1000.0 * m;
static const float m2 = m * m;
static const float m3 = m * m * m;
static const float pi = PI * rad;
static const float deg = pi / 180.0;
static const float watt_per_square_meter = watt / m2;
static const float watt_per_square_meter_per_sr = watt / (m2 * sr);
static const float watt_per_square_meter_per_nm = watt / (m2 * nm);
static const float watt_per_square_meter_per_sr_per_nm = watt / (m2 * sr * nm);
static const float watt_per_cubic_meter_per_sr_per_nm = watt / (m3 * sr * nm);
static const float cd = lm / sr;
static const float kcd = 1000.0 * cd;
static const float cd_per_square_meter = cd / m2;
static const float kcd_per_square_meter = kcd / m2;

struct DensityProfileLayer {
    float width;
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
};

struct DensityProfile {
    DensityProfileLayer layers[2];
};

struct AtmosphereParameters {
    float3 solar_irradiance;
    float sun_angular_radius;
    float bottom_radius;
    float top_radius;
    DensityProfile rayleigh_density;
    float3 rayleigh_scattering;
    DensityProfile mie_density;
    float3 mie_scattering;
    float3 mie_extinction;
    float mie_phase_function_g;
    DensityProfile absorption_density;
    float3 absorption_extinction;
    float3 ground_albedo;
    float mu_s_min;
};

static const AtmosphereParameters ATMOSPHERE = {
    float3(1.474000, 1.850400, 1.911980),
    0.004675,
    6360.000000,
    6420.000000,
    { { { 0.000000, 0.000000, 0.000000, 0.000000, 0.000000 }, { 0.000000, 1.000000, -0.125000, 0.000000, 0.000000 } } },
    float3(0.005802, 0.013558, 0.033100),
    { { { 0.000000, 0.000000, 0.000000, 0.000000, 0.000000 }, { 0.000000, 1.000000, -0.833333, 0.000000, 0.000000 } } },
    float3(0.003996, 0.003996, 0.003996),
    float3(0.004440, 0.004440, 0.004440),
    0.800000,
    { { { 25.000000, 0.000000, 0.000000, 0.066667, -0.666667 }, { 0.000000, 0.000000, 0.000000, -0.066667, 2.666667 } } },
    float3(0.000650, 0.001881, 0.000085),
    float3(0.100000, 0.100000, 0.100000),
    -0.500000
};

static const float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(683.000000, 683.000000, 683.000000);
static const float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(98242.786222, 69954.398112, 66475.012354);
static const float kLengthUnitInMeters = 1000.0f;
static const float3 kSphereCenter = float3(0.0, 0.0, 1.0); // this reduces to 0, 0, 1..... float3(0.0, 0.0, 1000.0) / kLengthUnitInMeters;
static const float kSphereRadius = 1.0; // this reduces to 1.... 1000.0 / kLengthUnitInMeters;
static const float3 kSphereAlbedo = float3(0.8, 0.8, 0.8); // NOTE: Change these albedos later for diff ground colors and for moon
static const float3 kGroundAlbedo = float3(0.0, 0.0, 0.04);

//--------------------------
// TEXTURES AND SAMPLERS
//--------------------------
Texture2D transmittance_texture : register(t0);
Texture2D irradiance_texture : register(t1);
Texture3D scattering_texture : register(t2);
SamplerState linearSampler : register(s0);

//----------------------------
// GENERAL UTILITY FUNCTIONS
//----------------------------
float ClampCosine(float mu)
{
    return clamp(mu, float(-1.0), float(1.0));
}

float ClampDistance(float d)
{
    return max(d, 0.0 * m);
}

float ClampRadius(const AtmosphereParameters atmosphere,
float r)
{
    return clamp(r, atmosphere.bottom_radius, atmosphere.top_radius);
}

float SafeSqrt(float a)
{
    return sqrt(max(a, 0.0 * m2));
}

//---------------------------------------------
// ATMOSPHERIC SCATTERING COMPUTATION FUNCTIONS
//---------------------------------------------

// called by GetTransmittanceTextureUvFromRMu
// called by GetScatteringTextureUvwzFromRMuMuSNu
float DistanceToTopAtmosphereBoundary(const AtmosphereParameters atmosphere,
    float r, float mu)
{
    float discriminant = r * r * (mu * mu - 1.0) +
        atmosphere.top_radius * atmosphere.top_radius;
    return ClampDistance(-r * mu + SafeSqrt(discriminant));
}

// called by GetSkyRadianceToPoint
// called by GetSkyRadiance
bool RayIntersectsGround(const AtmosphereParameters atmosphere,
    float r, float mu)
{
    return mu < 0.0 && r * r * (mu * mu - 1.0) +
        atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0 * m2;
}

// called by GetSkyRadiance
float RayleighPhaseFunction(float nu)
{
    float k = 3.0 / (16.0 * PI * sr);
    return k * (1.0 + nu * nu);
}

// called by GetSkyRadianceToPoint
// called by GetSkyRadiance
float MiePhaseFunction(float g, float nu)
{
    float k = 3.0 / (8.0 * PI * sr) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}

// called by GetIrradianceTextureUvFromRMuS
// called by GetTransmittanceTextureUvFromRMu
// called by GetScatteringTextureUvwzFromRMuMuSNu
float GetTextureCoordFromUnitRange(float x, int texture_size)
{
    return 0.5 / float(texture_size) + x * (1.0 - 1.0 / float(texture_size));
}

// called by GetIrradiance
float2 GetIrradianceTextureUvFromRMuS(const AtmosphereParameters atmosphere,
float r, float mu_s) {
float x_r = (r - atmosphere.bottom_radius) /
        (atmosphere.top_radius - atmosphere.bottom_radius);
float x_mu_s = mu_s * 0.5 + 0.5;
    return float2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
                  GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
}

// called by GetTransmittanceToTopAtmosphereBoundary
float2 GetTransmittanceTextureUvFromRMu(const AtmosphereParameters atmosphere,
    float r, float mu)
{
    float H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
        atmosphere.bottom_radius * atmosphere.bottom_radius);
    float rho =
        SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    float d = DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
    float d_min = atmosphere.top_radius - r;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;
    return float2(GetTextureCoordFromUnitRange(x_mu, TRANSMITTANCE_TEXTURE_WIDTH),
                  GetTextureCoordFromUnitRange(x_r, TRANSMITTANCE_TEXTURE_HEIGHT));
}

// called by GetCombinedScattering
float4 GetScatteringTextureUvwzFromRMuMuSNu(
    const AtmosphereParameters atmosphere,
    float r, float mu, float mu_s, float nu,
    bool ray_r_mu_intersects_ground) {
    float H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
            atmosphere.bottom_radius * atmosphere.bottom_radius);
    float rho = SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    float u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);
    float r_mu = r * mu;
    float discriminant = r_mu * r_mu - r * r + atmosphere.bottom_radius * atmosphere.bottom_radius;
    float u_mu;
    if (ray_r_mu_intersects_ground) {
        float d = -r_mu - SafeSqrt(discriminant);
        float d_min = r - atmosphere.bottom_radius;
        float d_max = rho;
                u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :
                    (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
    } else {
        float d = -r_mu + SafeSqrt(discriminant + H * H);
        float d_min = atmosphere.top_radius - r;
        float d_max = rho + H;
                u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange(
                    (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
    }
    float d = DistanceToTopAtmosphereBoundary(atmosphere, atmosphere.bottom_radius, mu_s);
    float d_min = atmosphere.top_radius - atmosphere.bottom_radius;
    float d_max = H;
    float a = (d - d_min) / (d_max - d_min);
    float D = DistanceToTopAtmosphereBoundary(atmosphere, atmosphere.bottom_radius, atmosphere.mu_s_min);
    float A = (D - d_min) / (d_max - d_min);
    float u_mu_s = GetTextureCoordFromUnitRange(max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);
    float u_nu = (nu + 1.0) / 2.0;
    
    return float4(u_nu, u_mu_s, u_mu, u_r);
}

// called by GetTransmittanceToSun
// called by GetTransmittance
// called by GetSkyRadiance
float3 GetTransmittanceToTopAtmosphereBoundary(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    float r, float mu) {
    float2 uv = GetTransmittanceTextureUvFromRMu(atmosphere, r, mu);
    return float3(transmittance_tex.Sample(linearSampler, uv).rgb);
}

// called by GetSkyRadianceToPoint
// called by GetSkyRadiance
float3 GetTransmittance(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    float r, float mu, float d, bool ray_r_mu_intersects_ground) {
    float r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
    float mu_d = ClampCosine((r * mu + d) / r_d);
    if (ray_r_mu_intersects_ground) {
        return min(
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r_d, -mu_d) /
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r, -mu),
            float3(1.0, 1.0, 1.0));
    } else {
        return min(
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r, mu) /
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r_d, mu_d),
            float3(1.0, 1.0, 1.0));
    }
}

// called by GetSunAndSkyIrradiance
float3 GetTransmittanceToSun(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    float r, float mu_s) {
    float sin_theta_h = atmosphere.bottom_radius / r;
    float cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
    return GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_tex, r, mu_s) *
        smoothstep(-sin_theta_h * atmosphere.sun_angular_radius / rad,
                   sin_theta_h * atmosphere.sun_angular_radius / rad,
                   mu_s - cos_theta_h);
}

// called by GetCombinedScattering
// called by GetSkyRadianceToPoint
float3 GetExtrapolatedSingleMieScattering(
    const AtmosphereParameters atmosphere, const float4 scattering) {
    if (scattering.r <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }
    return scattering.rgb * scattering.a / scattering.r *
        (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *
        (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
}

// called by GetSkyRadianceToPoint
// called by GetSkyRadiance
float3 GetCombinedScattering(
    const AtmosphereParameters atmosphere,
    const Texture3D scattering_tex,
    float r, float mu, float mu_s, float nu,
    bool ray_r_mu_intersects_ground, out float3 single_mie_scattering) {
    float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
        atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
    float tex_coord_x = uvwz.x * float(SCATTERING_TEXTURE_NU_SIZE - 1);
    float tex_x = floor(tex_coord_x);
    float lerp_factor = tex_coord_x - tex_x;
    float3 uvw0 = float3((tex_x + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE),
        uvwz.z, uvwz.w);
    float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE),
        uvwz.z, uvwz.w);
    float4 combined_scattering =
        scattering_tex.Sample(linearSampler, uvw0) * (1.0 - lerp_factor) +
        scattering_tex.Sample(linearSampler, uvw1) * lerp_factor;
    float3 scattering = float3(combined_scattering.rgb);
    single_mie_scattering = GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);
    return scattering;
}

// called by GetSkyLuminance
float3 GetSkyRadiance(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    const Texture3D scattering_tex,
    float3 camera_pos, const float3 view_ray, float shadow_length,
    const float3 sun_dir, out float3 transmittance) {
    float r = length(camera_pos);
    float rmu = dot(camera_pos, view_ray);
    float distance_to_top_atmosphere_boundary = -rmu -
        sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);
    if (distance_to_top_atmosphere_boundary > 0.0 * m) {
        camera_pos = camera_pos + view_ray * distance_to_top_atmosphere_boundary;
        r = atmosphere.top_radius;
        rmu += distance_to_top_atmosphere_boundary;
    } else if (r > atmosphere.top_radius) {
        transmittance = float3(1.0, 1.0, 1.0);
        float w = 0.0 * watt_per_square_meter_per_sr_per_nm;
        return float3(w,w,w);
    }
    float mu = rmu / r;
    float mu_s = dot(camera_pos, sun_dir) / r;
    float nu = dot(view_ray, sun_dir);
    bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);
    transmittance = ray_r_mu_intersects_ground ? float3(0.0, 0.0, 0.0) :
        GetTransmittanceToTopAtmosphereBoundary(atmosphere, transmittance_tex, r, mu);
    float3 single_mie_scattering;
    float3 scattering;
    if (shadow_length == 0.0 * m) {
        scattering = GetCombinedScattering(
            atmosphere, scattering_tex,
            r, mu, mu_s, nu, ray_r_mu_intersects_ground,
            single_mie_scattering);
    } else {
        float d = shadow_length;
        float r_p =
            ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
        float mu_p = (r * mu + d) / r_p;
        float mu_s_p = (r * mu_s + d * nu) / r_p;
        scattering = GetCombinedScattering(
            atmosphere, scattering_tex,
            r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
            single_mie_scattering);
        float3 shadow_transmittance =
            GetTransmittance(atmosphere, transmittance_tex,
                r, mu, shadow_length, ray_r_mu_intersects_ground);
        scattering = scattering * shadow_transmittance;
        single_mie_scattering = single_mie_scattering * shadow_transmittance;
    }
    return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
        MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
}

// called by GetSkyLuminanceToPoint
float3 GetSkyRadianceToPoint(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    const Texture3D scattering_tex,
    float3 camera_pos, const float3 point_pos, float shadow_length,
    const float3 sun_dir, out float3 transmittance) {
    float3 view_ray = normalize(point_pos - camera_pos);
    float r = length(camera_pos);
    float rmu = dot(camera_pos, view_ray);
    float distance_to_top_atmosphere_boundary = -rmu -
        sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);
    if (distance_to_top_atmosphere_boundary > 0.0 * m) {
        camera_pos = camera_pos + view_ray * distance_to_top_atmosphere_boundary;
        r = atmosphere.top_radius;
        rmu += distance_to_top_atmosphere_boundary;
    }
    float mu = rmu / r;
    float mu_s = dot(camera_pos, sun_dir) / r;
    float nu = dot(view_ray, sun_dir);
    float d = length(point_pos - camera_pos);
    bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);
    transmittance = GetTransmittance(atmosphere, transmittance_tex,
        r, mu, d, ray_r_mu_intersects_ground);
    float3 single_mie_scattering;
    float3 scattering = GetCombinedScattering(
        atmosphere, scattering_tex,
        r, mu, mu_s, nu, ray_r_mu_intersects_ground,
        single_mie_scattering);
    d = max(d - shadow_length, 0.0 * m);
    float r_p = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
    float mu_p = (r * mu + d) / r_p;
    float mu_s_p = (r * mu_s + d * nu) / r_p;
    float3 single_mie_scattering_p;
    float3 scattering_p = GetCombinedScattering(
        atmosphere, scattering_tex,
        r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
        single_mie_scattering_p);
    float3 shadow_transmittance = transmittance;
    if (shadow_length > 0.0 * m) {
        shadow_transmittance = GetTransmittance(atmosphere, transmittance_tex,
            r, mu, d, ray_r_mu_intersects_ground);
    }
    scattering = scattering - shadow_transmittance * scattering_p;
    single_mie_scattering =
        single_mie_scattering - shadow_transmittance * single_mie_scattering_p;
    single_mie_scattering = GetExtrapolatedSingleMieScattering(
        atmosphere, float4(scattering, single_mie_scattering.r));
    single_mie_scattering = single_mie_scattering *
        smoothstep(float(0.0), float(0.01), mu_s);
    return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
        MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
}

// called by GetSunAndSkyIrradiance
float3 GetIrradiance(
    const AtmosphereParameters atmosphere,
    const Texture2D irradiance_tex,
    float r, float mu_s) {
    float2 uv = GetIrradianceTextureUvFromRMuS(atmosphere, r, mu_s);
    return float3(irradiance_tex.Sample(linearSampler, uv).rgb);
}

// called by GetSunAndSkyIlluminance
float3 GetSunAndSkyIrradiance(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    const Texture2D irradiance_tex,
    const float3 point_pos, const float3 norm, const float3 sun_dir,
    out float3 sky_irradiance) {
    float r = length(point_pos);
    float mu_s = dot(point_pos, sun_dir) / r;
    sky_irradiance = GetIrradiance(atmosphere, irradiance_tex, r, mu_s) *
        (1.0 + dot(norm, point_pos) / r) * 0.5;
    return atmosphere.solar_irradiance *
        GetTransmittanceToSun(
            atmosphere, transmittance_tex, r, mu_s) *
        max(dot(norm, sun_dir), 0.0);
}

//----------------------------
// WRAPPER FUNCTIONS
//----------------------------
float3 GetSolarLuminance() {
    return ATMOSPHERE.solar_irradiance /
        (PI * ATMOSPHERE.sun_angular_radius * ATMOSPHERE.sun_angular_radius) * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

float3 GetSkyLuminance(
    float3 camera_pos, float3 view_ray, float shadow_length,
    float3 sun_dir, out float3 transmittance) {
    return GetSkyRadiance(ATMOSPHERE, transmittance_texture,
        scattering_texture, camera_pos, view_ray, shadow_length, sun_dir, transmittance) * SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

float3 GetSkyLuminanceToPoint(
    float3 camera_pos, float3 point_pos, float shadow_length,
    float3 sun_dir, out float3 transmittance) {
    return GetSkyRadianceToPoint(ATMOSPHERE, transmittance_texture,
        scattering_texture, camera_pos, point_pos, shadow_length, sun_dir, transmittance) * SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

float3 GetSunAndSkyIlluminance(
    float3 p, float3 norm, float3 sun_dir, out float3 sky_irradiance) {
    float3 sun_irradiance = GetSunAndSkyIrradiance(
      ATMOSPHERE, transmittance_texture, irradiance_texture, p, norm,
      sun_dir, sky_irradiance);
    sky_irradiance *= SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
    return sun_irradiance * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

float GetSunVisibility(float3 point_pos, float3 sun_dir) {
    float3 p = point_pos - kSphereCenter;
    float p_dot_v = dot(p, sun_dir);
    float p_dot_p = dot(p, p);
    float ray_sphere_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
    float discriminant =
        kSphereRadius * kSphereRadius - ray_sphere_center_squared_distance;
    if (discriminant >= 0.0) {
        float distance_to_intersection = -p_dot_v - sqrt(discriminant);
        if (distance_to_intersection > 0.0) {
            float ray_sphere_distance =
                kSphereRadius - sqrt(ray_sphere_center_squared_distance);
            float ray_sphere_angular_distance = -ray_sphere_distance / p_dot_v;
            return smoothstep(1.0, 0.0, ray_sphere_angular_distance / sun_size.x);
        }
    }
    return 1.0;
}

float GetSkyVisibility(float3 point_pos) {
    float3 p = point_pos - kSphereCenter;
    float p_dot_p = dot(p, p);
    return
        1.0 + p.z / sqrt(p_dot_p) * kSphereRadius * kSphereRadius / p_dot_p;
}

void GetSphereShadowInOut(float3 view_direction, float3 sun_dir,
    out float d_in, out float d_out) {
    //float3 camera = float3(invView[0][3], invView[1][3], invView[2][3]); // from the 4th column instead of row..
    //camera *= (1.0 / 1000.0);
    float3 pos = camera - kSphereCenter;
    float pos_dot_sun = dot(pos, sun_dir);
    float view_dot_sun = dot(view_direction, sun_dir);
    float k = sun_size.x;
    float l = 1.0 + k * k;
    float a = 1.0 - l * view_dot_sun * view_dot_sun;
    float b = dot(pos, view_direction) - l * pos_dot_sun * view_dot_sun -
        k * kSphereRadius * view_dot_sun;
    float c = dot(pos, pos) - l * pos_dot_sun * pos_dot_sun -
        2.0 * k * kSphereRadius * pos_dot_sun - kSphereRadius * kSphereRadius;
    float discriminant = b * b - a * c;
    if (discriminant > 0.0) {
        d_in = max(0.0, (-b - sqrt(discriminant)) / a);
        d_out = (-b + sqrt(discriminant)) / a;
        float d_base = -pos_dot_sun / view_dot_sun;
        float d_apex = -(pos_dot_sun + kSphereRadius / k) / view_dot_sun;
        if (view_dot_sun > 0.0) {
            d_in = max(d_in, d_apex);
            d_out = a > 0.0 ? min(d_out, d_base) : d_base;
        } else {
            d_in = a > 0.0 ? max(d_in, d_base) : d_base;
            d_out = min(d_out, d_apex);
        }
    } else {
        d_in = 0.0;
        d_out = 0.0;
    }
}

// Pixel Shader Input
struct PSInput
{
    float4 position : SV_POSITION;
    float3 view_ray : TEXCOORD0;
};

//------------------
// MAIN
//------------------
float4 main(PSInput input) : SV_TARGET
{
    float4 color;

    float3 view_direction = normalize(input.view_ray);
    float fragment_angular_size =
        length(ddx(input.view_ray) + ddy(input.view_ray)) / length(input.view_ray);
    float shadow_in;
    float shadow_out;
    GetSphereShadowInOut(view_direction, sun_direction, shadow_in, shadow_out);
    float lightshaft_fadein_hack = smoothstep(
        0.02, 0.04, dot(normalize(camera - earth_center), sun_direction));
    
    // Test sphere S intersection
    float3 p = camera - kSphereCenter;
    float p_dot_v = dot(p, view_direction);
    float p_dot_p = dot(p, p);
    float ray_sphere_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
    float discriminant =
        kSphereRadius * kSphereRadius - ray_sphere_center_squared_distance;
    float sphere_alpha = 0.0;
    float3 sphere_radiance = float3(0.0, 0.0, 0.0);
    if (discriminant >= 0.0) {
        float distance_to_intersection = -p_dot_v - sqrt(discriminant);
        if (distance_to_intersection > 0.0) {
            float ray_sphere_distance =
                kSphereRadius - sqrt(ray_sphere_center_squared_distance);
            float ray_sphere_angular_distance = -ray_sphere_distance / p_dot_v;
            sphere_alpha =
                min(ray_sphere_angular_distance / fragment_angular_size, 1.0);
            
            float3 point_pos = camera + view_direction * distance_to_intersection;
            float3 norm = normalize(point_pos - kSphereCenter);
            float3 sky_irradiance;
            float3 sun_irradiance = GetSunAndSkyIlluminance(
                point_pos - earth_center, norm, sun_direction, sky_irradiance);
            sphere_radiance =
                kSphereAlbedo * (1.0 / PI) * (sun_irradiance + sky_irradiance);
            
            float shadow_length =
                max(0.0, min(shadow_out, distance_to_intersection) - shadow_in) *
                lightshaft_fadein_hack;
            float3 transmittance;
            float3 in_scatter = GetSkyLuminanceToPoint(camera - earth_center,
                point_pos - earth_center, shadow_length, sun_direction, transmittance);
            sphere_radiance = sphere_radiance * transmittance + in_scatter;
        }
    }
    
    // Test planet sphere P intersection
    p = camera - earth_center;
    p_dot_v = dot(p, view_direction);
    p_dot_p = dot(p, p);
    float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
    discriminant =
        earth_center.z * earth_center.z - ray_earth_center_squared_distance;
    float ground_alpha = 0.0;
    float3 ground_radiance = float3(0.0, 0.0, 0.0);
    if (discriminant >= 0.0) {
        float distance_to_intersection = -p_dot_v - sqrt(discriminant);
        if (distance_to_intersection > 0.0) {
            float3 point_pos = camera + view_direction * distance_to_intersection;
            float3 norm = normalize(point_pos - earth_center);
            float3 sky_irradiance;
            float3 sun_irradiance = GetSunAndSkyIlluminance(
                point_pos - earth_center, norm, sun_direction, sky_irradiance);
            ground_radiance = kGroundAlbedo * (1.0 / PI) * (
                sun_irradiance * GetSunVisibility(point_pos, sun_direction) +
                sky_irradiance * GetSkyVisibility(point_pos));
            float shadow_length =
                max(0.0, min(shadow_out, distance_to_intersection) - shadow_in) *
                lightshaft_fadein_hack;
            float3 transmittance;
            float3 in_scatter = GetSkyLuminanceToPoint(camera - earth_center,
                point_pos - earth_center, shadow_length, sun_direction, transmittance);
            ground_radiance = ground_radiance * transmittance + in_scatter;
            ground_alpha = 1.0;
        }
    }
    
    // Compute sky radiance
    float shadow_length = max(0.0, shadow_out - shadow_in) *
        lightshaft_fadein_hack;
    float3 transmittance;
    float3 radiance = GetSkyLuminance(
        camera - earth_center, view_direction, shadow_length, sun_direction,
        transmittance);
    if (dot(view_direction, sun_direction) > sun_size.y) {
        radiance = radiance + transmittance * GetSolarLuminance();
    }
    
    radiance = lerp(radiance, ground_radiance, ground_alpha);
    radiance = lerp(radiance, sphere_radiance, sphere_alpha);
    color.rgb =
        pow(float3(1.0, 1.0, 1.0) - exp(-radiance / white_point * exposure), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    color.a = 1.0;
    
    return color;
}