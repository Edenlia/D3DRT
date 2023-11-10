cbuffer CameraParams : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewInv;
    float4x4 projectionInv;
}

cbuffer WorldMatrix : register(b1)
{
    float4x4 world;
}

struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    
    float4 ndcPos = input.position;
    ndcPos = mul(world, ndcPos);
    ndcPos = mul(view, ndcPos);
    ndcPos = mul(projection, ndcPos);
    result.position = ndcPos;
    result.color = input.color;
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}