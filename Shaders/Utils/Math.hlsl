
// wi and wo are direct out of the surface
float3 reflect(float3 wi, float3 N)
{
    return 2 * dot(wi, N) * N - wi;
}