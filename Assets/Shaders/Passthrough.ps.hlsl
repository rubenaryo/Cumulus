Texture2D gInput : register(t0);
Texture2D depthStencilBuffer : register(t1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer GodRayCB : register(b0)
{
    float4 lightScreenPos;     // 0–1 screen space
    float exposure;            // intensity multiplier
    float decay;               // ray attenuation
    float density;             // sample density
    float weight;              // sample weight
    int numSamples;            // radial blur samples
    float padding1;
    float padding2;
    float padding3;
};

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;

    float2 deltaTexCoord = (lightScreenPos.xy - uv) * density;

    float2 texCoord = uv;
    float illumination = 0.0f;
    float currentDecay = 1.0f;

    [loop]
    for (int i = 0; i < numSamples; ++i)
    {
        texCoord += deltaTexCoord;

        float occlusion = gInput.SampleLevel(gsamLinearClamp, texCoord, 0).r;

        illumination += occlusion * currentDecay * weight;
        currentDecay *= decay;
    }

    float3 rays = illumination * exposure;

    float3 baseColor = gInput.SampleLevel(gsamPointClamp, uv, 0).rgb;

    float radius = 0.01f; // how big the debug circle is
    float dist = distance(uv, lightScreenPos.xy);

    if (dist < radius)
    {
        // draw solid red
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    return float4(baseColor, 1.0f);
  //  return float4(baseColor + rays, 1.0f);
}

