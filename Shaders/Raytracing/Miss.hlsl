#include "Common.hlsl"

[shader("miss")]
export void Miss(inout HitInfo payload : SV_RayPayload)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    float ramp = launchIndex.y / dims.y;
    
    float3 skyColor = float3(1.0f, 1.0f, 1.0f) * (ramp) + float3(.5, .7, 1.0) * (1-ramp);
    
    payload.color = float4(skyColor, 1);
    payload.depth = 999;
}