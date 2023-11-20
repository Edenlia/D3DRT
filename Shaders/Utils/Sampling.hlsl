#include "Math.hlsl"

#define PI 3.141592653589793
#define PI2 6.283185307179586

float rand_1to1(float x)
{
    // -1 -1
    return frac(sin(x) * 10000.0);
}

float rand_2to1(float2 uv)
{
    // 0 - 1
    const float a = 12.9898, b = 78.233, c = 43758.5453;
    float dt = dot(uv.xy, float2(a, b)), sn = fmod(dt, PI);
    return frac(sin(sn) * c);
}

uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

// Generate ith frame b bounce light's random seed
float2 sobolSeed(const in uint i, const in uint b)
{
    float u = sobol(b * 2, grayCode(i));
    float v = sobol(b * 2 + 1, grayCode(i));
    return float2(u, v);
}

float2 randomSeed(const in float2 uv)
{
    return float2(rand_1to1(uv.x), rand_1to1(uv.y));
}

float2 CranleyPattersonRotation(float2 seed, float2 rayIndex)
{
    uint pseed = uint(
        uint(rayIndex.x) * uint(1973) +
        uint(rayIndex.y) * uint(9277) +
        uint(114514 / 1919) * uint(26699)) | uint(1);
    
    float u = float(wang_hash(pseed)) / 4294967296.0;
    float v = float(wang_hash(pseed)) / 4294967296.0;

    seed.x += u;
    if (seed.x > 1)
        seed.x -= 1;
    if (seed.x < 0)
        seed.x += 1;

    seed.y += v;
    if (seed.y > 1)
        seed.y -= 1;
    if (seed.y < 0)
        seed.y += 1;

    return seed;

}

float3 sphereSample(const in float2 seed)
{
    float z = 1.0 - 2.0 * seed.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2 * PI * seed.y;
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, z);
}

float3 toNormalHemisphere(float3 v, const in float3x3 TBN)
{
    return normalize(mul(TBN, v));
}

float3 hemisphereSample(const in float3x3 TBN, const in float2 seed)
{
    float3 sample = sphereSample(seed);
    if (sample.z < 0.0)
    {
        sample.z *= -1.0;
    }
    
    return toNormalHemisphere(sample, TBN);
}

// https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#Cosine-WeightedHemisphereSampling
float3 cosineHemisphereSample(const in float3x3 TBN, const in float2 seed)
{
    float r = sqrt(seed.x);
    float theta = seed.y * 2.0 * PI;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0 - x * x - y * y);
    
    return toNormalHemisphere(float3(x, y, z), TBN);
}

float3 GTR2Sample(const in float3x3 TBN, const in float2 seed, const in float alpha, const in float3 V)
{
    float phi_h = 2.0 * PI * seed.x;
    float sin_phi_h = sin(phi_h);
    float cos_phi_h = cos(phi_h);

    float cos_theta_h = sqrt((1.0 - seed.y) / (1.0 + (alpha * alpha - 1.0) * seed.y));
    float sin_theta_h = sqrt(max(0.0, 1.0 - cos_theta_h * cos_theta_h));
    
    float3 H = float3(sin_theta_h * cos_phi_h, sin_theta_h * sin_phi_h, cos_theta_h);
    H = toNormalHemisphere(H, TBN);
    
    return reflect(-V, H);
}

float3 GTR1Sample(const in float3x3 TBN, const in float2 seed, const in float alpha, const in float3 V)
{
    float phi_h = 2.0 * PI * seed.x;
    float sin_phi_h = sin(phi_h);
    float cos_phi_h = cos(phi_h);

    float cos_theta_h = sqrt((1.0 - pow(alpha * alpha, 1.0 - seed.y)) / (1.0 - alpha * alpha));
    float sin_theta_h = sqrt(max(0.0, 1.0 - cos_theta_h * cos_theta_h));
    
    float3 H = float3(sin_theta_h * cos_phi_h, sin_theta_h * sin_phi_h, cos_theta_h);
    H = toNormalHemisphere(H, TBN);
    
    return reflect(-V, H);

}