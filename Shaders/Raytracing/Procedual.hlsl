#include "Common.hlsl"

struct Ray
{
    float3 origin;
    float3 direction;
};

void swap(inout float a, inout float b)
{
    float temp = a;
    a = b;
    b = temp;
}

// Solve a quadratic equation.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveQuadraticEqn(float a, float b, float c, out float x0, out float x1)
{
    float discr = b * b - 4 * a * c;
    if (discr < 0)
        return false;
    else if (discr == 0)
        x0 = x1 = -0.5 * b / a;
    else
    {
        float q = (b > 0) ?
            -0.5 * (b + sqrt(discr)) :
            -0.5 * (b - sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }
    if (x0 > x1)
        swap(x0, x1);

    return true;
}

// Analytic solution of an unbounded ray sphere intersection points.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveRaySphereIntersectionEquation(in Ray ray, out float tmin, out float tmax, in float3 center, in float radius)
{
    float3 L = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2 * dot(ray.direction, L);
    float c = dot(L, L) - radius * radius;
    return SolveQuadraticEqn(a, b, c, tmin, tmax);
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow sphere.
bool RaySphereIntersectionTest(in Ray ray, out float thit, out float tmax, in float3 center = float3(0, 0, 0), in float radius = 1)
{
    float t0, t1; // solutions for t if the ray intersects 

    if (!SolveRaySphereIntersectionEquation(ray, t0, t1, center, radius))
        return false;
    tmax = t1;

    if (t0 < RayTMin())
    {
        // t0 is before RayTMin, let's use t1 instead .
        if (t1 < RayTMin())
            return false; // both t0 and t1 are before RayTMin
        
        thit = t1;
        return true;
    }
    else
    {
        thit = t0;
        return true;
    }
    return false;
}

[shader("intersection")]
void SphereIntersection()
{    
    Ray ray;
    ray.origin = WorldRayOrigin();
    ray.direction = WorldRayDirection();
    
    ProcedualGeometryAttributes attr;
    
    ReportHit(RayTMin(), /*hitKind*/0, attr);
    
    int radius = 3;
    float3 center = float3(0, 0, 0);
        
    float thit, tmax;
    if (RaySphereIntersectionTest(ray, thit, tmax, center, radius))
    {
        ReportHit(thit, /*hitKind*/0, attr);
    }
}

[shader("closesthit")]
void SphereClosestHit(inout HitInfo payload, ProcedualGeometryAttributes attrib)
{
    float3 hitColor = float3(1.0, 0.0, 0.0);
    
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}