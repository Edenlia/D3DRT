#define PI 3.141592653589793
#define PI2 6.283185307179586
#define NUM_SAMPLES 4
#define NUM_RINGS 5 // Poisson disk'else ring count

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

float2 randomSeed(const in float2 uv)
{
    return float2(rand_1to1(uv.x), rand_1to1(uv.y));
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

float3 hemisphereSample(const in float3 normal, const in float2 seed)
{
    float3 sample = sphereSample(seed);
    
    return dot(normal, sample) < 0 ? -sample : sample;
}