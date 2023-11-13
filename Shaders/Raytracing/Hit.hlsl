#include "Common.hlsl"
#include "../Utils/Sampling.hlsl"

// #DXR Extra: Per-Instance Data
cbuffer Colors : register(b0)
{
    float3 A;
    float3 B;
    float3 C;
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

float3 HitAttribute(float3 attrib[3], float3 barycentrics)
{
    return attrib[0] * barycentrics.x + attrib[1] * barycentrics.y + attrib[2] * barycentrics.z;
}

[shader("closesthit")] 
export void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    
    uint baseIndex = PrimitiveIndex() * 3;
    
    if (payload.depth < 4)
    {
        float3 vertexNormals[3] =
        {
            vertices[indices[baseIndex]].normal,
            vertices[indices[baseIndex + 1]].normal,
            vertices[indices[baseIndex + 2]].normal
        };
    
    
        float3 hitNormal = HitAttribute(vertexNormals, barycentrics);
        
        uint sampleCount = 2;
        float2 seed = float2(attrib.bary.x + ObjectRayDirection().x, attrib.bary.y + ObjectRayDirection().y);
        float2 seeds[4];
        
        seeds[0] = randomSeed(seed);
        seeds[1] = randomSeed(seeds[0]);
        seeds[2] = randomSeed(seeds[1]);
        seeds[3] = randomSeed(seeds[2]);
        
        // Bounce the ray
        float3 color = float3(0, 0, 0);
        
        for (uint i = 0; i < sampleCount; i++)
        {
            float2 seed = randomSeed(seed);
            float3 bounceDir = hemisphereSample(hitNormal, seeds[i]);
            bounceDir = normalize(bounceDir);
    
            RayDesc ray;
            ray.Origin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
            ray.Direction = bounceDir;
            ray.TMin = 0.01;
            ray.TMax = 1000;
    
            HitInfo bouncePayload;
            bouncePayload.depth = payload.depth + 1;
    
    
            TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            0,
            0,
            ray,
            bouncePayload);
            
            color += bouncePayload.colorAndDistance.xyz * 0.5;
        }
        
        color /= sampleCount;
    
        payload.colorAndDistance = float4(color, RayTCurrent());
    }
    else
    {
        payload.colorAndDistance = float4(0, 0, 0, 0);
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
    payload.colorAndDistance = float4(hitColor);
}