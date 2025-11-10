// Atmosphere Shader - HLSL (DirectX 12)
#define USE_LUMINANCE
#define IN(x) const x
#define OUT(x) out x
#define TEMPLATE(x)
#define TEMPLATE_ARGUMENT(x)

static const int TRANSMITTANCE_TEXTURE_WIDTH = 256;
static const int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
static const int SCATTERING_TEXTURE_R_SIZE = 32;
static const int SCATTERING_TEXTURE_MU_SIZE = 128;
static const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
static const int SCATTERING_TEXTURE_NU_SIZE = 8;
static const int IRRADIANCE_TEXTURE_WIDTH = 64;
static const int IRRADIANCE_TEXTURE_HEIGHT = 16;

#define COMBINED_SCATTERING_TEXTURES

// Type definitions
#define Length float
#define Wavelength float
#define Angle float
#define SolidAngle float
#define Power float
#define LuminousPower float
#define Number float
#define InverseLength float
#define Area float
#define Volume float
#define NumberDensity float
#define Irradiance float
#define Radiance float
#define SpectralPower float
#define SpectralIrradiance float
#define SpectralRadiance float
#define SpectralRadianceDensity float
#define ScatteringCoefficient float
#define InverseSolidAngle float
#define LuminousIntensity float
#define Luminance float
#define Illuminance float
#define AbstractSpectrum float3
#define DimensionlessSpectrum float3
#define PowerSpectrum float3
#define IrradianceSpectrum float3
#define RadianceSpectrum float3
#define RadianceDensitySpectrum float3
#define ScatteringSpectrum float3
#define Position float3
#define Direction float3
#define Luminance3 float3
#define Illuminance3 float3
#define TransmittanceTexture Texture2D
#define AbstractScatteringTexture Texture3D
#define ReducedScatteringTexture Texture3D
#define ScatteringTexture Texture3D
#define ScatteringDensityTexture Texture3D
#define IrradianceTexture Texture2D

static const Length m = 1.0;
static const Wavelength nm = 1.0;
static const Angle rad = 1.0;
static const SolidAngle sr = 1.0;
static const Power watt = 1.0;
static const LuminousPower lm = 1.0;
static const float PI = 3.14159265358979323846;
static const Length km = 1000.0 * m;
static const Area m2 = m * m;
static const Volume m3 = m * m * m;
static const Angle pi = PI * rad;
static const Angle deg = pi / 180.0;
static const Irradiance watt_per_square_meter = watt / m2;
static const Radiance watt_per_square_meter_per_sr = watt / (m2 * sr);
static const SpectralIrradiance watt_per_square_meter_per_nm = watt / (m2 * nm);
static const SpectralRadiance watt_per_square_meter_per_sr_per_nm = watt / (m2 * sr * nm);
static const SpectralRadianceDensity watt_per_cubic_meter_per_sr_per_nm = watt / (m3 * sr * nm);
static const LuminousIntensity cd = lm / sr;
static const LuminousIntensity kcd = 1000.0 * cd;
static const Luminance cd_per_square_meter = cd / m2;
static const Luminance kcd_per_square_meter = kcd / m2;

struct DensityProfileLayer {
    Length width;
    Number exp_term;
    InverseLength exp_scale;
    InverseLength linear_term;
    Number constant_term;
};

struct DensityProfile {
    DensityProfileLayer layers[2];
};

struct AtmosphereParameters {
    IrradianceSpectrum solar_irradiance;
    Angle sun_angular_radius;
    Length bottom_radius;
    Length top_radius;
    DensityProfile rayleigh_density;
    ScatteringSpectrum rayleigh_scattering;
    DensityProfile mie_density;
    ScatteringSpectrum mie_scattering;
    ScatteringSpectrum mie_extinction;
    Number mie_phase_function_g;
    DensityProfile absorption_density;
    ScatteringSpectrum absorption_extinction;
    DimensionlessSpectrum ground_albedo;
    Number mu_s_min;
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
static const float kLengthUnitInMeters = 1000.000000;

// Constant buffer
cbuffer AtmosphereConstants : register(b0)
{
    float3 camera;
    float exposure;
    float3 white_point;
    float pad0;
    float3 earth_center;
    float pad1;
    float3 sun_direction;
    float pad2;
    float2 sun_size;
    int display_texture;
    int scatter_slice;
};

// Textures and samplers
Texture2D transmittance_texture : register(t0);
Texture2D irradiance_texture : register(t1);
Texture3D scattering_texture : register(t2);
SamplerState linearSampler : register(s0);

static const float3 kSphereCenter = float3(0.0, 0.0, 1000.0) / kLengthUnitInMeters;
static const float kSphereRadius = 1000.0 / kLengthUnitInMeters;
static const float3 kSphereAlbedo = float3(0.8, 0.8, 0.8);
static const float3 kGroundAlbedo = float3(0.0, 0.0, 0.04);

// Utility functions
Number ClampCosine(Number mu) {
    return clamp(mu, Number(-1.0), Number(1.0));
}

Length ClampDistance(Length d) {
    return max(d, 0.0 * m);
}

Length ClampRadius(IN(AtmosphereParameters) atmosphere, Length r) {
    return clamp(r, atmosphere.bottom_radius, atmosphere.top_radius);
}

Length SafeSqrt(Area a) {
    return sqrt(max(a, 0.0 * m2));
}

Length DistanceToTopAtmosphereBoundary(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu) {
    Area discriminant = r * r * (mu * mu - 1.0) +
        atmosphere.top_radius * atmosphere.top_radius;
    return ClampDistance(-r * mu + SafeSqrt(discriminant));
}

bool RayIntersectsGround(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu) {
    return mu < 0.0 && r * r * (mu * mu - 1.0) +
        atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0 * m2;
}

Number GetLayerDensity(IN(DensityProfileLayer) layer, Length altitude) {
    Number density = layer.exp_term * exp(layer.exp_scale * altitude) +
        layer.linear_term * altitude + layer.constant_term;
    return clamp(density, Number(0.0), Number(1.0));
}

Number GetProfileDensity(IN(DensityProfile) profile, Length altitude) {
    return altitude < profile.layers[0].width ?
        GetLayerDensity(profile.layers[0], altitude) :
        GetLayerDensity(profile.layers[1], altitude);
}

Number GetTextureCoordFromUnitRange(Number x, int texture_size) {
    return 0.5 / Number(texture_size) + x * (1.0 - 1.0 / Number(texture_size));
}

float2 GetTransmittanceTextureUvFromRMu(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu) {
    Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
        atmosphere.bottom_radius * atmosphere.bottom_radius);
    Length rho =
        SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    Length d = DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
    Length d_min = atmosphere.top_radius - r;
    Length d_max = rho + H;
    Number x_mu = (d - d_min) / (d_max - d_min);
    Number x_r = rho / H;
    return float2(GetTextureCoordFromUnitRange(x_mu, TRANSMITTANCE_TEXTURE_WIDTH),
                  GetTextureCoordFromUnitRange(x_r, TRANSMITTANCE_TEXTURE_HEIGHT));
}

DimensionlessSpectrum GetTransmittanceToTopAtmosphereBoundary(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_tex,
    Length r, Number mu) {
    float2 uv = GetTransmittanceTextureUvFromRMu(atmosphere, r, mu);
    return DimensionlessSpectrum(transmittance_tex.Sample(linearSampler, uv).rgb);
}

DimensionlessSpectrum GetTransmittance(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_tex,
    Length r, Number mu, Length d, bool ray_r_mu_intersects_ground) {
    Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
    Number mu_d = ClampCosine((r * mu + d) / r_d);
    if (ray_r_mu_intersects_ground) {
        return min(
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r_d, -mu_d) /
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r, -mu),
            DimensionlessSpectrum(1.0, 1.0, 1.0));
    } else {
        return min(
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r, mu) /
            GetTransmittanceToTopAtmosphereBoundary(
                atmosphere, transmittance_tex, r_d, mu_d),
            DimensionlessSpectrum(1.0, 1.0, 1.0));
    }
}

DimensionlessSpectrum GetTransmittanceToSun(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_tex,
    Length r, Number mu_s) {
    Number sin_theta_h = atmosphere.bottom_radius / r;
    Number cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
    return GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_tex, r, mu_s) *
        smoothstep(-sin_theta_h * atmosphere.sun_angular_radius / rad,
                   sin_theta_h * atmosphere.sun_angular_radius / rad,
                   mu_s - cos_theta_h);
}

InverseSolidAngle RayleighPhaseFunction(Number nu) {
    InverseSolidAngle k = 3.0 / (16.0 * PI * sr);
    return k * (1.0 + nu * nu);
}

InverseSolidAngle MiePhaseFunction(Number g, Number nu) {
    InverseSolidAngle k = 3.0 / (8.0 * PI * sr) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}

float4 GetScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground) {
    Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
        atmosphere.bottom_radius * atmosphere.bottom_radius);
    Length rho =
        SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    Number u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);
    Length r_mu = r * mu;
    Area discriminant =
        r_mu * r_mu - r * r + atmosphere.bottom_radius * atmosphere.bottom_radius;
    Number u_mu;
    if (ray_r_mu_intersects_ground) {
        Length d = -r_mu - SafeSqrt(discriminant);
        Length d_min = r - atmosphere.bottom_radius;
        Length d_max = rho;
        u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :
            (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
    } else {
        Length d = -r_mu + SafeSqrt(discriminant + H * H);
        Length d_min = atmosphere.top_radius - r;
        Length d_max = rho + H;
        u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange(
            (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
    }
    Length d = DistanceToTopAtmosphereBoundary(
        atmosphere, atmosphere.bottom_radius, mu_s);
    Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;
    Length d_max = H;
    Number a = (d - d_min) / (d_max - d_min);
    Length D = DistanceToTopAtmosphereBoundary(
        atmosphere, atmosphere.bottom_radius, atmosphere.mu_s_min);
    Number A = (D - d_min) / (d_max - d_min);
    Number u_mu_s = GetTextureCoordFromUnitRange(
        max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);
    Number u_nu = (nu + 1.0) / 2.0;
    return float4(u_nu, u_mu_s, u_mu, u_r);
}

float3 GetExtrapolatedSingleMieScattering(
    IN(AtmosphereParameters) atmosphere, IN(float4) scattering) {
    if (scattering.r <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }
    return scattering.rgb * scattering.a / scattering.r *
        (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *
        (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
}

IrradianceSpectrum GetCombinedScattering(
    IN(AtmosphereParameters) atmosphere,
    IN(ReducedScatteringTexture) scattering_tex,
    IN(ReducedScatteringTexture) single_mie_scattering_tex,
    Length r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground,
    OUT(IrradianceSpectrum) single_mie_scattering) {
    float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
        atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
    Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
    Number tex_x = floor(tex_coord_x);
    Number lerp_factor = tex_coord_x - tex_x;
    float3 uvw0 = float3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
        uvwz.z, uvwz.w);
    float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
        uvwz.z, uvwz.w);
    float4 combined_scattering =
        scattering_tex.Sample(linearSampler, uvw0) * (1.0 - lerp_factor) +
        scattering_tex.Sample(linearSampler, uvw1) * lerp_factor;
    IrradianceSpectrum scattering = IrradianceSpectrum(combined_scattering.rgb);
    single_mie_scattering =
        GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);
    return scattering;
}

RadianceSpectrum GetSkyRadiance(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_tex,
    IN(ReducedScatteringTexture) scattering_tex,
    IN(ReducedScatteringTexture) single_mie_scattering_tex,
    Position camera_pos, IN(Direction) view_ray, Length shadow_length,
    IN(Direction) sun_dir, OUT(DimensionlessSpectrum) transmittance) {
    Length r = length(camera_pos);
    Length rmu = dot(camera_pos, view_ray);
    Length distance_to_top_atmosphere_boundary = -rmu -
        sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);
    if (distance_to_top_atmosphere_boundary > 0.0 * m) {
        camera_pos = camera_pos + view_ray * distance_to_top_atmosphere_boundary;
        r = atmosphere.top_radius;
        rmu += distance_to_top_atmosphere_boundary;
    } else if (r > atmosphere.top_radius) {
        transmittance = DimensionlessSpectrum(1.0, 1.0, 1.0);
        float w = 0.0 * watt_per_square_meter_per_sr_per_nm;
        return RadianceSpectrum(w,w,w);
    }
    Number mu = rmu / r;
    Number mu_s = dot(camera_pos, sun_dir) / r;
    Number nu = dot(view_ray, sun_dir);
    bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);
    transmittance = ray_r_mu_intersects_ground ? DimensionlessSpectrum(0.0, 0.0, 0.0) :
        GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_tex, r, mu);
    IrradianceSpectrum single_mie_scattering;
    IrradianceSpectrum scattering;
    if (shadow_length == 0.0 * m) {
        scattering = GetCombinedScattering(
            atmosphere, scattering_tex, single_mie_scattering_tex,
            r, mu, mu_s, nu, ray_r_mu_intersects_ground,
            single_mie_scattering);
    } else {
        Length d = shadow_length;
        Length r_p =
            ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
        Number mu_p = (r * mu + d) / r_p;
        Number mu_s_p = (r * mu_s + d * nu) / r_p;
        scattering = GetCombinedScattering(
            atmosphere, scattering_tex, single_mie_scattering_tex,
            r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
            single_mie_scattering);
        DimensionlessSpectrum shadow_transmittance =
            GetTransmittance(atmosphere, transmittance_tex,
                r, mu, shadow_length, ray_r_mu_intersects_ground);
        scattering = scattering * shadow_transmittance;
        single_mie_scattering = single_mie_scattering * shadow_transmittance;
    }
    return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
        MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
}

RadianceSpectrum GetSkyRadianceToPoint(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_tex,
    IN(ReducedScatteringTexture) scattering_tex,
    IN(ReducedScatteringTexture) single_mie_scattering_tex,
    Position camera_pos, IN(Position) point_pos, Length shadow_length,
    IN(Direction) sun_dir, OUT(DimensionlessSpectrum) transmittance) {
    Direction view_ray = normalize(point_pos - camera_pos);
    Length r = length(camera_pos);
    Length rmu = dot(camera_pos, view_ray);
    Length distance_to_top_atmosphere_boundary = -rmu -
        sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);
    if (distance_to_top_atmosphere_boundary > 0.0 * m) {
        camera_pos = camera_pos + view_ray * distance_to_top_atmosphere_boundary;
        r = atmosphere.top_radius;
        rmu += distance_to_top_atmosphere_boundary;
    }
    Number mu = rmu / r;
    Number mu_s = dot(camera_pos, sun_dir) / r;
    Number nu = dot(view_ray, sun_dir);
    Length d = length(point_pos - camera_pos);
    bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);
    transmittance = GetTransmittance(atmosphere, transmittance_tex,
        r, mu, d, ray_r_mu_intersects_ground);
    IrradianceSpectrum single_mie_scattering;
    IrradianceSpectrum scattering = GetCombinedScattering(
        atmosphere, scattering_tex, single_mie_scattering_tex,
        r, mu, mu_s, nu, ray_r_mu_intersects_ground,
        single_mie_scattering);
    d = max(d - shadow_length, 0.0 * m);
    Length r_p = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
    Number mu_p = (r * mu + d) / r_p;
    Number mu_s_p = (r * mu_s + d * nu) / r_p;
    IrradianceSpectrum single_mie_scattering_p;
    IrradianceSpectrum scattering_p = GetCombinedScattering(
        atmosphere, scattering_tex, single_mie_scattering_tex,
        r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
        single_mie_scattering_p);
    DimensionlessSpectrum shadow_transmittance = transmittance;
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
        smoothstep(Number(0.0), Number(0.01), mu_s);
    return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
        MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
}

float2 GetIrradianceTextureUvFromRMuS(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu_s) {
    Number x_r = (r - atmosphere.bottom_radius) /
        (atmosphere.top_radius - atmosphere.bottom_radius);
    Number x_mu_s = mu_s * 0.5 + 0.5;
    return float2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
                  GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
}

IrradianceSpectrum GetIrradiance(
    IN(AtmosphereParameters) atmosphere,
    IN(IrradianceTexture) irradiance_tex,
    Length r, Number mu_s) {
    float2 uv = GetIrradianceTextureUvFromRMuS(atmosphere, r, mu_s);
    return IrradianceSpectrum(irradiance_tex.Sample(linearSampler, uv).rgb);
}

IrradianceSpectrum GetSunAndSkyIrradiance(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_tex,
    IN(IrradianceTexture) irradiance_tex,
    IN(Position) point_pos, IN(Direction) norm, IN(Direction) sun_dir,
    OUT(IrradianceSpectrum) sky_irradiance) {
    Length r = length(point_pos);
    Number mu_s = dot(point_pos, sun_dir) / r;
    sky_irradiance = GetIrradiance(atmosphere, irradiance_tex, r, mu_s) *
        (1.0 + dot(norm, point_pos) / r) * 0.5;
    return atmosphere.solar_irradiance *
        GetTransmittanceToSun(
            atmosphere, transmittance_tex, r, mu_s) *
        max(dot(norm, sun_dir), 0.0);
}

// Wrapper functions
float3 GetSolarLuminance() {
    return ATMOSPHERE.solar_irradiance /
        (PI * ATMOSPHERE.sun_angular_radius * ATMOSPHERE.sun_angular_radius);
}

float3 GetSkyLuminance(
    float3 camera_pos, float3 view_ray, float shadow_length,
    float3 sun_dir, out float3 transmittance) {
    return GetSkyRadiance(ATMOSPHERE, transmittance_texture,
        scattering_texture, scattering_texture,
        camera_pos, view_ray, shadow_length, sun_dir, transmittance);
}

float3 GetSkyLuminanceToPoint(
    float3 camera_pos, float3 point_pos, float shadow_length,
    float3 sun_dir, out float3 transmittance) {
    return GetSkyRadianceToPoint(ATMOSPHERE, transmittance_texture,
        scattering_texture, scattering_texture,
        camera_pos, point_pos, shadow_length, sun_dir, transmittance);
}

float3 GetSunAndSkyIlluminance(
    float3 p, float3 norm, float3 sun_dir, out float3 sky_irradiance) {
    return GetSunAndSkyIrradiance(
        ATMOSPHERE, transmittance_texture, irradiance_texture,
        p, norm, sun_dir, sky_irradiance);
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

// Pixel Shader
float4 main(PSInput input) : SV_TARGET
{
    float4 color;
    
    if (display_texture == 0)
    {
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
    }
    else if (display_texture == 1) {
        float2 texCoord = input.position.xy / float2(256.0, 64.0);
        color = transmittance_texture.Sample(linearSampler, texCoord);
    }
    else if (display_texture == 2) {
        float2 texCoord = input.position.xy / float2(64.0, 16.0);
        color = irradiance_texture.Sample(linearSampler, texCoord);
    }
    else if (display_texture == 3) {
        float3 texCoord = float3(input.position.xy / float2(8.0 * 32.0, 128.0), scatter_slice * 1.0);
        color = scattering_texture.Sample(linearSampler, texCoord);
    }
    
    return color;
}