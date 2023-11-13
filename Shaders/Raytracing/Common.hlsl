// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct Vertex
{
    float3 position;
    float4 color;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bitangent;
};

struct HitInfo
{
    float4 colorAndDistance;
    float3 normal;
    uint depth;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
  float2 bary;
};

struct ProcedualGeometryAttributes
{
    float dummy;
};