#include "Common.hlsl"
#include "../Utils/Sampling.hlsl"

// #DXR Extra: Per-Instance Data
cbuffer GlobalParams : register(b0)
{
    uint frameCount;
}

// #DXR Extra - Another ray type
struct ShadowHitInfo
{
    bool isHit;
};

StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<int> indices: register(t1);

// #DXR Extra - Another ray type
// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t2);

cbuffer DisneyMaterialParams : register(b1)
{
    float4 baseColor;
    float metallic;
    float subsurface;
    float specular;
    float roughness;
    float specularTint;
    float anisotropic;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatGloss;
}

float3 HitAttribute(float3 attrib[3], float3 barycentrics)
{
    return attrib[0] * barycentrics.x + attrib[1] * barycentrics.y + attrib[2] * barycentrics.z;
}

float2 HitAttribute2(float2 attrib[3], float3 barycentrics)
{
    return attrib[0] * barycentrics.x + attrib[1] * barycentrics.y + attrib[2] * barycentrics.z;
}

float SchlickFresnel(float u)
{
    float m = clamp(1 - u, 0, 1);
    float m2 = m * m;
    return m2 * m2 * m; // pow(m,5)
}

float GTR1(float NdotH, float a)
{
    if (a >= 1)
        return 1 / PI;
    float a2 = a * a;
    float t = 1 + (a2 - 1) * NdotH * NdotH;
    return (a2 - 1) / (PI * log(a2) * t);
}

float GTR2(float NdotH, float a)
{
    float a2 = a * a;
    float t = 1 + (a2 - 1) * NdotH * NdotH;
    return a2 / (PI * t * t);
}

float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NdotV * NdotV;
    return 1 / (NdotV + sqrt(a + b - a * b));
}


// Gamma
float3 mon2lin(float3 x)
{
    return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

float3 Disney_BRDF(float3 L, float3 V, float3 N, out float pdfL)
{
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if (NdotL < 0 || NdotV < 0)
        return float3(0, 0, 0);

    float3 H = normalize(L + V);
    float NdotH = dot(N, H);
    float LdotH = dot(L, H);
    
    float3 Cdlin = baseColor.xyz; // mon2lin(baseColor.xyz);
    float Cdlum = .3 * Cdlin.x + .6 * Cdlin.y + .1 * Cdlin.z; // luminance approx.
    float3 Ctint = Cdlum > 0 ? Cdlin / Cdlum : float3(1, 1, 1); // normalize lum. to isolate hue+sat
    
    
    float FL = SchlickFresnel(NdotL);
    float FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * NdotH * NdotH * roughness;
    float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
    
    // subsurface scattering
    float Fss90 = LdotH * LdotH * roughness;
    float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (NdotL + NdotV) - 0.5) + 0.5);
    
    float3 L_diffuse = lerp(Fd, ss, subsurface) * Cdlin / PI;
    
    
    float3 Cspec = specular * lerp(float3(1, 1, 1), Ctint, specularTint);
    float3 Cspec0 = lerp(0.08 * Cspec, Cdlin, metallic);
    
    
    // Specular
    float alpha = max(0.001, roughness * roughness);
    float Ds = GTR2(NdotH, alpha);
    float FH = SchlickFresnel(LdotH);
    float3 Fs = lerp(Cspec0, float3(1, 1, 1), FH);
    float Gs = smithG_GGX(NdotL, roughness) * smithG_GGX(NdotV, roughness);
    float3 L_specular = Ds * Fs * Gs; // don't need to / (4 * NdotL * NdotV)
    
    // Clearcoat
    float Dr = GTR1(NdotH, lerp(0.1, 0.001, clearcoatGloss));
    float Fr = lerp(0.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, 0.25) * smithG_GGX(NdotV, 0.25);
    float3 L_clearcoat = Dr * Fr * Gr;
    
    // Sheen
    float3 Csheen = lerp(float3(1, 1, 1), Ctint, sheenTint);
    float3 Fsheen = FH * sheen * Csheen;
    L_diffuse += Fsheen;
    
    // specular sampling test
    float pdfH = Ds * NdotH;
    pdfL = pdfH / (4.0 * LdotH);
    // return L_specular;
    
    // diffuse sampling test
    pdfL = NdotL / PI;
    return L_diffuse;
    
    return L_diffuse * (1.0 - metallic) + L_specular + L_clearcoat * 0.25 * clearcoat;
}

[shader("closesthit")] 
export void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    
    uint baseIndex = PrimitiveIndex() * 3;
        
    float3 vertexNormals[3] =
    {
        vertices[indices[baseIndex]].normal,
        vertices[indices[baseIndex + 1]].normal,
        vertices[indices[baseIndex + 2]].normal
    };
    float3 hitNormal = HitAttribute(vertexNormals, barycentrics);
    
    float3 vertexTangents[3] =
    {
        vertices[indices[baseIndex]].tangent,
        vertices[indices[baseIndex + 1]].tangent,
        vertices[indices[baseIndex + 2]].tangent
    };
    float3 hitTangent = HitAttribute(vertexTangents, barycentrics);
    
    float3 vertexBitangents[3] =
    {
        vertices[indices[baseIndex]].bitangent,
        vertices[indices[baseIndex + 1]].bitangent,
        vertices[indices[baseIndex + 2]].bitangent
    };
    float3 hitBitangent = HitAttribute(vertexBitangents, barycentrics);
    
    float2 vertexUVs[3] =
    {
        vertices[indices[baseIndex]].uv,
        vertices[indices[baseIndex + 1]].uv,
        vertices[indices[baseIndex + 2]].uv
    };
    float2 hitUV = HitAttribute2(vertexUVs, barycentrics);
            
    float3x3 TBN = transpose(float3x3(hitTangent, hitBitangent, hitNormal)); // each axis should be a column
    
    if (payload.depth < 4)
    {        
        uint2 launchIndex = DispatchRaysIndex().xy; // pixel coordinate [0, 1920), [0, 1080)
        float2 dims = float2(DispatchRaysDimensions().xy); // resolution: 1920x1080
        
        float2 seed;
        
        if (launchIndex.x < dims.x / 2)
        {
            seed = float2(attrib.bary.x + ObjectRayDirection().x, attrib.bary.y + ObjectRayDirection().y);
            seed *= frameCount + 1;
            seed = randomSeed(seed);
        }
        else
        {
            seed = sobolSeed(frameCount, payload.depth);
            seed = CranleyPattersonRotation(seed, launchIndex);
        }
        
        float3 color = float3(0, 0, 0);
        
        float3 wo = normalize(-WorldRayDirection());
        float alpha = max(0.001, roughness * roughness);
            
        float3 bounceDir = hemisphereSample(TBN, seed);
        // bounceDir = cosineHemisphereSample(TBN, seed);
        // bounceDir = GTR2Sample(TBN, seeds[i], alpha, wo);
        bounceDir = normalize(bounceDir);
        
        RayDesc ray;
        ray.Origin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
        ray.Direction = bounceDir;
        ray.TMin = 0.01;
        ray.TMax = 1000;
        
        HitInfo bouncePayload;
        bouncePayload.depth = payload.depth + 1;
            
        float3 wi = bounceDir;
        float3 n = hitNormal;
        float cosI = dot(hitNormal, bounceDir);
        float pdf = 1.0 / (2.0 * PI);
        float a = 0;
            
        float3 brdf = Disney_BRDF(wi, wo, n, a);
            
        TraceRay(
        SceneBVH,
        RAY_FLAG_NONE,
        0xFF,
        0,
        0,
        0,
        ray,
        bouncePayload);
            
        color += bouncePayload.color.xyz * brdf * cosI / pdf;
        
        
        payload.color = float4(color, 1);

    }
    else
    {
        payload.color = float4(0, 0, 0, 1);
    }
}

// #DXR Extra: Per-Instance Data
[shader("closesthit")]
void PlaneClosestHit(inout HitInfo payload, Attributes attrib)
{
    float3 lightPos = float3(2, 2, -2);
    
    // Find the world - space hit position
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    float3 lightDir = normalize(lightPos - worldOrigin);
    
      // Fire a shadow ray. The direction is hard-coded here, but can be fetched
  // from a constant-buffer
    RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = lightDir;
    ray.TMin = 0.01;
    ray.TMax = 100000;
    bool hit = true;

  // Initialize the ray payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;

  // Trace the ray
    TraceRay(
      // Acceleration structure
      SceneBVH,
      // Flags can be used to specify the behavior upon hitting a surface
      RAY_FLAG_NONE,
      // Instance inclusion mask, which can be used to mask out some geometry to
      // this ray by and-ing the mask with a geometry mask. The 0xFF flag then
      // indicates no geometry will be masked
      0xFF,
      // Depending on the type of ray, a given object can have several hit
      // groups attached (ie. what to do when hitting to compute regular
      // shading, and what to do when hitting to compute shadows). Those hit
      // groups are specified sequentially in the SBT, so the value below
      // indicates which offset (on 4 bits) to apply to the hit groups for this
      // ray. In this sample we only have one hit group per object, hence an
      // offset of 0.
      1,
      // The offsets in the SBT can be computed from the object ID, its instance
      // ID, but also simply by the order the objects have been pushed in the
      // acceleration structure. This allows the application to group shaders in
      // the SBT in the same order as they are added in the AS, in which case
      // the value below represents the stride (4 bits representing the number
      // of hit groups) between two consecutive objects.
      0,
      // Index of the miss shader to use in case several consecutive miss
      // shaders are present in the SBT. This allows to change the behavior of
      // the program when no geometry have been hit, for example one to return a
      // sky color for regular rendering, and another returning a full
      // visibility value for shadow rays. This sample has only one miss shader,
      // hence an index 0
      1,
      // Ray information to trace
      ray,
      // Payload associated to the ray, which will be used to communicate
      // between the hit/miss shaders and the raygen
      shadowPayload);

    float factor = shadowPayload.isHit ? 0.6 : 1.0;

    float3 barycentrics =
      float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float4 hitColor = float4(float3(0.7, 0.7, 0.3) * factor, RayTCurrent());
    payload.color = hitColor;
}