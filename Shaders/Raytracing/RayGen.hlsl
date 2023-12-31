#include "Common.hlsl"
#include "../Utils/Sampling.hlsl"

#define RAY_DEPTH 4
#define RAY_NUM 2
#define NUM_SAMPLES 2
#define NUM_RINGS 4 // Poisson disk'else ring count
#define SSP_MAX 1e10

// Raytracing output texture, accessed as a UAV
RWTexture2D< float4 > gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D frameTexture : register(t1);

// #DXR Extra: Perspective Camera
cbuffer CameraParams : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewI;
    float4x4 projectionI;
}

cbuffer GlobalParams : register(b1)
{
    uint frameCount;
}

void poissonDiskSamples(const in float2 randomSeed, inout float2 poissonDisk[])
{

    float ANGLE_STEP = PI2 * float(NUM_RINGS) / float(NUM_SAMPLES);
    float INV_NUM_SAMPLES = 1.0 / float(NUM_SAMPLES);

    float angle = rand_2to1(randomSeed) * PI2;
    float radius = INV_NUM_SAMPLES;
    float radiusStep = radius;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        poissonDisk[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.75);
        radius += radiusStep;
        angle += ANGLE_STEP;
    }
}

[shader("raygeneration")] 
export void RayGen() {
    // Initialize the ray payload
    HitInfo payload;
    payload.depth = 0;

    // Get the location within the dispatched 2D grid of work items
    // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy; // pixel coordinate [0, 1920), [0, 1080)
    float2 dims = float2(DispatchRaysDimensions().xy); // resolution: 1920x1080
    float aspectRatio = dims.x / dims.y;
    float2 uv = (launchIndex.xy + 0.5f) / dims.xy;
        
    // float2 poissonDisk[NUM_SAMPLES];
    // poissonDiskSamples(uv, poissonDisk);
    
    float3 color = float3(0, 0, 0);
    
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
    
    // Perspective
    RayDesc ray;
    ray.Origin = mul(viewI, float4(0, 0, 0, 1)).xyz; // World space origin
    float4 dir = mul(projectionI, float4(d.x, -d.y, 1, 1)); // NDC space direction to world space. d.y is flipped screen space left top origin, ndc is left bottom origin
    ray.Direction = mul(viewI, float4(dir.xyz, 0)).xyz;
    ray.TMin = 0;
    ray.TMax = 100000;
                
    // Trace the ray
    TraceRay(
    // Parameter name: AccelerationStructure
    // Acceleration structure
    SceneBVH,

    // Parameter name: RayFlags
    // Flags can be used to specify the behavior upon hitting a surface
    RAY_FLAG_NONE,

    // Parameter name: InstanceInclusionMask
    // Instance inclusion mask, which can be used to mask out some geometry to this ray by
    // and-ing the mask with a geometry mask. The 0xFF flag then indicates no geometry will be
    // masked
    0xFF,

    // Parameter name: RayContributionToHitGroupIndex
    // Depending on the type of ray, a given object can have several hit groups attached
    // (ie. what to do when hitting to compute regular shading, and what to do when hitting
    // to compute shadows). Those hit groups are specified sequentially in the SBT, so the value
    // below indicates which offset (on 4 bits) to apply to the hit groups for this ray. In this
    // sample we only have one hit group per object, hence an offset of 0.
    0,

    // Parameter name: MultiplierForGeometryContributionToHitGroupIndex
    // The offsets in the SBT can be computed from the object ID, its instance ID, but also simply
    // by the order the objects have been pushed in the acceleration structure. This allows the
    // application to group shaders in the SBT in the same order as they are added in the AS, in
    // which case the value below represents the stride (4 bits representing the number of hit
    // groups) between two consecutive objects.
    0,

    // Parameter name: MissShaderIndex
    // Index of the miss shader to use in case several consecutive miss shaders are present in the
    // SBT. This allows to change the behavior of the program when no geometry have been hit, for
    // example one to return a sky color for regular rendering, and another returning a full
    // visibility value for shadow rays. This sample has only one miss shader, hence an index 0
    0,

    // Parameter name: Ray
    // Ray information to trace
    ray,

    // Parameter name: Payload
    // Payload associated to the ray, which will be used to communicate between the hit/miss
    // shaders and the raygen
    payload);
        
    color += payload.color.xyz;
    
    float4 frameColor = frameTexture[launchIndex];
    
    if (frameCount < SSP_MAX)
    {
        gOutput[launchIndex] = (frameColor * float(frameCount) + float4(color, 1.f)) / float(frameCount + 1);
    }
    else
    {
        gOutput[launchIndex] = frameColor;
    }
    
    if (SSP_MAX == 0)
    {
        gOutput[launchIndex] = float4(color, 1.f);
    }

}
