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
// These textures come from running his OpenGL version with the same settings as our


#include "AtmosphereBuffer.hlsli"

#define RENDERSPHERE 0

//---------------------------
// CONSTANTS AND DEFINITIONS
//---------------------------

// Texture dimensions
static const int TRANSMITTANCE_TEXTURE_WIDTH = 256;
static const int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
static const int SCATTERING_TEXTURE_R_SIZE = 32;
static const int SCATTERING_TEXTURE_MU_SIZE = 128;
static const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
static const int SCATTERING_TEXTURE_NU_SIZE = 8;
static const int IRRADIANCE_TEXTURE_WIDTH = 64;
static const int IRRADIANCE_TEXTURE_HEIGHT = 16;

// Units for physical quantities
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

// An atmosphere layer of width 'width', and whose density is defined as
// 'exp_term' * exp('exp_scale' * h) + 'linear_term' * h + 'constant_term',
// clamped to [0,1], and where h is the altitude.
struct DensityProfileLayer {
    float width;
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
};

// An atmosphere density profile made of several layers on top of each other
// (from bottom to top). The width of the last layer is ignored, i.e. it always
// extend to the top atmosphere boundary. The profile values vary between 0
// (null density) to 1 (maximum density).
struct DensityProfile {
    DensityProfileLayer layers[2];
};

struct AtmosphereParameters
{
  // The solar irradiance at the top of the atmosphere.
    float3 solar_irradiance;
  // The sun's angular radius. Warning: the implementation uses approximations
  // that are valid only if this angle is smaller than 0.1 radians.
    float sun_angular_radius;
  // The distance between the planet center and the bottom of the atmosphere.
    float bottom_radius;
  // The distance between the planet center and the top of the atmosphere.
    float top_radius;
  // The density profile of air molecules, i.e. a function from altitude to
  // dimensionless values between 0 (null density) and 1 (maximum density).
    DensityProfile rayleigh_density;
  // The scattering coefficient of air molecules at the altitude where their
  // density is maximum (usually the bottom of the atmosphere), as a function of
  // wavelength. The scattering coefficient at altitude h is equal to
  // 'rayleigh_scattering' times 'rayleigh_density' at this altitude.
    float3 rayleigh_scattering;
  // The density profile of aerosols, i.e. a function from altitude to
  // dimensionless values between 0 (null density) and 1 (maximum density).
    DensityProfile mie_density;
  // The scattering coefficient of aerosols at the altitude where their density
  // is maximum (usually the bottom of the atmosphere), as a function of
  // wavelength. The scattering coefficient at altitude h is equal to
  // 'mie_scattering' times 'mie_density' at this altitude.
    float3 mie_scattering;
  // The extinction coefficient of aerosols at the altitude where their density
  // is maximum (usually the bottom of the atmosphere), as a function of
  // wavelength. The extinction coefficient at altitude h is equal to
  // 'mie_extinction' times 'mie_density' at this altitude.
    float3 mie_extinction;
  // The asymetry parameter for the Cornette-Shanks phase function for the
  // aerosols.
    float mie_phase_function_g;
  // The density profile of air molecules that absorb light (e.g. ozone), i.e.
  // a function from altitude to dimensionless values between 0 (null density)
  // and 1 (maximum density).
    DensityProfile absorption_density;
  // The extinction coefficient of molecules that absorb light (e.g. ozone) at
  // the altitude where their density is maximum, as a function of wavelength.
  // The extinction coefficient at altitude h is equal to
  // 'absorption_extinction' times 'absorption_density' at this altitude.
    float3 absorption_extinction;
  // The average albedo of the ground.
    float3 ground_albedo;
  // The cosine of the maximum Sun zenith angle for which atmospheric scattering
  // must be precomputed (for maximum precision, use the smallest Sun zenith
  // angle yielding negligible sky light radiance values. For instance, for the
  // Earth case, 102 degrees is a good choice - yielding mu_s_min = -0.2).
    float mu_s_min;
};

// These values were computed on CPU side in the original Bruneton code,
// and their results are copied here to simplicity.
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

// these top two values help with the precomputed illuminance values and are calculated on CPU in the Bruneton code
static const float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(683.000000, 683.000000, 683.000000);
static const float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(98242.786222, 69954.398112, 66475.012354);
static const float kLengthUnitInMeters = 1000.0f;
#if RENDERSPHERE
static const float3 kSphereCenter = float3(0.0, 0.0, 1.0); // this reduces to 0, 0, 1..... float3(0.0, 0.0, 1000.0) / kLengthUnitInMeters;
static const float kSphereRadius = 1.0; // this reduces to 1.... 1000.0 / kLengthUnitInMeters;
static const float3 kSphereAlbedo = float3(0.8, 0.8, 0.8); // NOTE: Change these albedos later for diff ground colors and for moon
#endif
static const float3 kGroundAlbedo = float3(0.0, 0.0, 0.04);

//--------------------------
// TEXTURES AND SAMPLERS
//--------------------------
Texture2D transmittance_texture : register(t0);
Texture2D irradiance_texture : register(t1);
Texture3D scattering_texture : register(t2);
SamplerState linearWrapSampler : register(s2);

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

/*
Note that we added the solar irradiance and the scattering coefficient terms
that we omitted in ComputeSingleScatteringIntegrand, but not the
phase function terms - they are added at render time
for better angular precision. We provide them here for completeness:
*/

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

/* Computing Transmittance to Top Atmosphere Boundary is quite costly.
Fortunately it depends on only two parameters and is quite smooth, so
we can precompute it in a small 2D texture to optimize its evaluation.
This becomes our precomputed Transmittance Texture.

For this we need mapping between the function parameters (r, mu)
and the texture coordinates (u, v),  because these parameters do
not have the same units and range of values.
And even if it was the case, storing a function from the [0, 1] interval in
a texture of size n would sample the function at 0.5, 1.5/n, ... (n-0.5)/n,
because texture samples are at the center oftexels. Therefore, this texture
would only give us extrapolated function values at the domain boundaries
(0 and 1). To avoid this we need to store f(0) at the center of texel 0 and
f(1) at the center of texel n-1. This can be done with the following mapping
from values x in [0,1] to texture coordinates u in [0.5/n,1-0.5/n]: */
// called by GetIrradianceTextureUvFromRMuS
// called by GetTransmittanceTextureUvFromRMu
// called by GetScatteringTextureUvwzFromRMuMuSNu
float GetTextureCoordFromUnitRange(float x, int texture_size)
{
    return 0.5 / float(texture_size) + x * (1.0 - 1.0 / float(texture_size));
}

/*
In order to precompute the ground irradiance in a texture we need a mapping
from the ground irradiance parameters to texture coordinates. Since we
precompute the ground irradiance only for horizontal surfaces, this irradiance
depends only on r and mu_s, so we need a mapping from (r,mu_s) to
(u,v) texture coordinates. The simplest, affine mapping is sufficient here,
because the ground irradiance function is very smooth: */
// called by GetIrradiance
float2 GetIrradianceTextureUvFromRMuS(const AtmosphereParameters atmosphere,
float r, float mu_s) {
float x_r = (r - atmosphere.bottom_radius) /
        (atmosphere.top_radius - atmosphere.bottom_radius);
float x_mu_s = mu_s * 0.5 + 0.5;
    return float2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
                  GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
}

// Same as above pretty much, but for transmittance
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

// Same as above pretty much, but for Scattering
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

/*
With the help of the precomputed texture, we can now get the
transmittance between a point and the top atmosphere boundary with a single
texture lookup (assuming there is no intersection with the ground): */
// called by GetTransmittanceToSun
// called by GetTransmittance
// called by GetSkyRadiance
float3 GetTransmittanceToTopAtmosphereBoundary(
    const AtmosphereParameters atmosphere,
    const Texture2D transmittance_tex,
    float r, float mu) {
    float2 uv = GetTransmittanceTextureUvFromRMu(atmosphere, r, mu);
    uv.y = 1.0 - uv.y;
    return float3(transmittance_tex.Sample(linearWrapSampler, uv).rgb);
}

/*
Also, with r_d=sqrt{d^2+2r\mu d+r^2} and mu_d=(r\mu+d)/r_d the values of
r and mu at bq, we can get the transmittance between two arbitrary
points bp and bq inside the atmosphere with only two texture lookups
(recall that the transmittance between bp and bq is the transmittance
between bp and the top atmosphere boundary, divided by the transmittance
between bq and the top atmosphere boundary, or the reverse - we continue to
assume that the segment between the two points does not intersect the ground):
*/
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

/*Where ray_r_mu_intersects_ground should be true if the ray
defined by r and mu intersects the ground. We don't compute it here with
RayIntersectsGround because the result could be wrong for rays
very close to the horizon, due to the finite precision and rounding errors of
floating point operations. And also because the caller generally has more robust
ways to know whether a ray intersects the ground or not (see below).

Finally, we will also need the transmittance between a point in the
atmosphere and the Sun. The Sun is not a point light source, so this is an
integral of the transmittance over the Sun disc. Here we consider that the
transmittance is constant over this disc, except below the horizon, where the
transmittance is 0. As a consequence, the transmittance to the Sun can be
computed with GetTransmittanceToTopAtmosphereBoundary, times the
fraction of the Sun disc which is above the horizon.

This fraction varies from 0 when the Sun zenith angle theta_s is larger
than the horizon zenith angle theta_h plus the Sun angular radius alpha_s,
to 1 when theta_s is smaller than theta_h-alpha_s. Equivalently, it
varies from 0 when mu_s=cos theta_s is smaller than
cos(theta_h+alpha_s)approx cos theta_h-alpha_s sin theta_h to 1 when
mu_s is larger than
cos(theta_h-alpha_s)approx cos theta_h+alpha_s sin theta_h. In between,
the visible Sun disc fraction varies approximately like a smoothstep (this can
be verified by plotting the area of  https://en.wikipedia.org/wiki/Circular_segment
as a function of its "https://en.wikipedia.org/wiki/Sagitta_(geometry). 
Therefore, since sin theta_h=r_{mathrm{bottom}}/r, we can
approximate the transmittance to the Sun with the following function:
*/
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

// Since we combined scattering textures, we need to extract the mie information from it
// However, it seems like scattering.a is just 1... so I am confusion, but it looks good.
// called by GetCombinedScattering
// called by GetSkyRadianceToPoint
float3 GetExtrapolatedSingleMieScattering(
    const AtmosphereParameters atmosphere, const float4 scattering) {
    if (scattering.r <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }
    return scattering.aaa * 0.1;
    return scattering.rgb * scattering.a / scattering.r *
        (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *
        (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
}

/*
We can then retrieve all the scattering components (Rayleigh + multiple
scattering on one side, and single Mie scattering on the other side) with the
following function, based on single_scattering_lookup (we duplicate
some code here, instead of using two calls to GetScattering, to
make sure that the texture coordinates computation is shared between the lookups
in scattering_texture and the mie extrapolation:
*/
// called by GetSkyRadianceToPoint
// called by GetSkyRadiance
float3 GetCombinedScattering(
    const AtmosphereParameters atmosphere,
    const Texture3D scattering_tex,
    float r, float mu, float mu_s, float nu,
    bool ray_r_mu_intersects_ground, out float3 single_mie_scattering) {
        float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
        float tex_coord_x = uvwz.x * float(SCATTERING_TEXTURE_NU_SIZE - 1);
        float tex_x = floor(tex_coord_x);
        float lerp_factor = tex_coord_x - tex_x;
        float3 uvw0 = float3((tex_x + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE),
            uvwz.z, uvwz.w);
        float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE),
            uvwz.z, uvwz.w);
        uvw0.y = 1.0 - uvw0.y;
        uvw1.y = 1.0 - uvw1.y;
    float4 tex0 = scattering_tex.Sample(linearWrapSampler, uvw0);
    float4 tex1 = scattering_tex.Sample(linearWrapSampler, uvw1);
    tex0.a = 1.0;
    tex1.a = 1.0;
        float4 combined_scattering =
            tex0 * (1.0 - lerp_factor) +
            tex1 * lerp_factor;
        float3 scattering = float3(combined_scattering.rgb);
        single_mie_scattering = GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);
    
    return scattering;
}

/*
To render the sky we simply need to display the sky radiance, which we can
get with a lookup in the precomputed scattering texture(s), multiplied by the
phase function terms that were omitted during precomputation. We can also return
the transmittance of the atmosphere (which we can get with a single lookup in
the precomputed transmittance texture), which is needed to correctly render the
objects in space (such as the Sun and the Moon). This leads to the following
function, where most of the computations are used to correctly handle the case
of viewers outside the atmosphere, and the case of light shafts:
*/
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

/*
To render the aerial perspective we need the transmittance and the scattering
between two points (i.e. between the viewer and a point on the ground, which can
at an arbibrary altitude). We already have a function to compute the
transmittance between two points (using 2 lookups in a texture which only
contains the transmittance to the top of the atmosphere), but we don't have one
for the scattering between 2 points. Hopefully, the scattering between 2 points
can be computed from two lookups in a texture which contains the scattering to
the nearest atmosphere boundary, as for the transmittance (except that here the
two lookup results must be subtracted, instead of divided). This is what we
implement in the following function (the initial computations are used to
correctly handle the case of viewers outside the atmosphere):
*/
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
    
    // artifact pr fix
    if (!ray_r_mu_intersects_ground)
    {
        float mu_horiz = -SafeSqrt(
        1.0 - (atmosphere.bottom_radius / r) * (atmosphere.bottom_radius / r));
        mu = max(mu, mu_horiz + 0.004);
    }
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

// The Bruneton comment here was 150 lines, so it is ommitted here for brevity.
// called by GetSunAndSkyIrradiance
float3 GetIrradiance(
    const AtmosphereParameters atmosphere,
    const Texture2D irradiance_tex,
    float r, float mu_s) {
    float2 uv = GetIrradianceTextureUvFromRMuS(atmosphere, r, mu_s);
    uv.y = 1.0 - uv.y;
    return float3(irradiance_tex.Sample(linearWrapSampler, uv).rgb);
}

/*
To render the ground we need the irradiance received on the ground after 0 or
more bounce(s) in the atmosphere or on the ground. The direct irradiance can be
computed with a lookup in the transmittance texture,
via GetTransmittanceToSun, while the indirect irradiance is given
by a lookup in the precomputed irradiance texture (this texture only contains
the irradiance for horizontal surfaces; we use the approximation defined in our
https://hal.inria.fr/inria-00288758/en paper for the other cases).
The function below returns the direct and indirect irradiances separately:
*/
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

#if RENDERSPHERE
/*
Testing if a point is in the shadow of the sphere S is equivalent to test if the
corresponding light ray intersects the sphere, which is very simple to do.
However, this is only valid for a punctual light source, which is not the case
of the Sun. In the following function we compute an approximate (and biased)
soft shadow by taking the angular size of the Sun into account:
*/
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

/*
The sphere also partially occludes the sky light, and we approximate this
effect with an ambient occlusion factor. The ambient occlusion factor due to a
sphere is given in http://webserver.dmt.upm.es/~isidoro/tc3/Radiation%20View%20factors.pdf
Radiation View Factors (Isidoro Martinez, 1995). In the simple case where
the sphere is fully visible, it is given by the following function:
*/
float GetSkyVisibility(float3 point_pos) {
    float3 p = point_pos - kSphereCenter;
    float p_dot_p = dot(p, p);
    return
        1.0 + p.z / sqrt(p_dot_p) * kSphereRadius * kSphereRadius / p_dot_p;
}

/*
To compute light shafts we need the intersections of the view ray with the
shadow volume of the sphere S. Since the Sun is not a punctual light source this
shadow volume is not a cylinder but a cone (for the umbra, plus another cone for
the penumbra, but we ignore it here):
*/
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
#endif

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
        length(abs(ddx(input.view_ray)) + abs(ddy(input.view_ray))) / length(input.view_ray);
    float shadow_in = 0.0f;
    float shadow_out = 0.0f;
    float lightshaft_fadein_hack = smoothstep(
        0.02, 0.04, dot(normalize(camera - earth_center), sun_direction));
    
#if RENDERSPHERE
    GetSphereShadowInOut(view_direction, sun_direction, shadow_in, shadow_out);

    float3 p = camera - kSphereCenter;
    float p_dot_v = dot(p, view_direction);
    float p_dot_p = dot(p, p);
    float ray_sphere_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
    float discriminant =
        kSphereRadius * kSphereRadius - ray_sphere_center_squared_distance;
    float sphere_alpha = 0.0;
    float3 sphere_radiance = float3(0.0, 0.0, 0.0);
    if (discriminant >= 0.0)
    {
        float distance_to_intersection = -p_dot_v - sqrt(discriminant);
        if (distance_to_intersection > 0.0)
        {
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
    p = camera - earth_center;
    p_dot_v = dot(p, view_direction);
    p_dot_p = dot(p, p);
    float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
    discriminant =
        earth_center.z * earth_center.z - ray_earth_center_squared_distance;
    
#else
    
    // Test planet sphere P intersection
    float3 p = camera - earth_center;
    float p_dot_v = dot(p, view_direction);
    float p_dot_p = dot(p, p);
    float ray_earth_center_squared_distance = p_dot_p - p_dot_v * p_dot_v;
    float discriminant =
        earth_center.z * earth_center.z - ray_earth_center_squared_distance;
#endif
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
#if RENDERSPHERE
            ground_radiance = kGroundAlbedo * (1.0 / PI) * (
                sun_irradiance * GetSunVisibility(point_pos, sun_direction) +
                sky_irradiance * GetSkyVisibility(point_pos));
            float shadow_length =
                max(0.0, min(shadow_out, distance_to_intersection) - shadow_in) *
                lightshaft_fadein_hack;
#else
            ground_radiance = kGroundAlbedo * (1.0 / PI) * sky_irradiance;
            float shadow_length = 0.0f;
#endif
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
#if RENDERSPHERE
    radiance = lerp(radiance, sphere_radiance, sphere_alpha);
#endif
    
    color.rgb =
        pow(float3(1.0, 1.0, 1.0) - exp(-radiance / white_point * exposure), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    color.a = 1.0;
    return color;
}