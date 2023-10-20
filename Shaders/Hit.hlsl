#include "Common.hlsl"

[shader("closesthit")] 
export void ClosestHit(inout HitInfo payload, Attributes attrib)
{
  payload.colorAndDistance = float4(1, 1, 0, RayTCurrent());
}
