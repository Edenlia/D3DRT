//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// #DXR Extra: Perspective Camera
cbuffer CameraParams : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewInv;
    float4x4 projectionInv;
}

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct BPPayload
{
    float4 worldPosition;
    float3 worldNormal;
    float3 cameraWorldPosition;
    float4 ndcPosition;
    float4 ndcNormal;
    float4 albedo;
};

struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct PSInput
{
    float4 worldPosition : WORLD_POSITION;
    float3 worldNormal : WORLD_NORMAL;
    float3 cameraWorldPosition : CAMERA_POSITION;
    float4 ndcPosition : SV_POSITION;
    float4 ndcNormal : NDC_NORMAL;
    float4 color : COLOR;
    float2 uv : UV;
};

float4 BlinnPhong(BPPayload payload)
{
    float3 lightPos = float3(3.0, 0.0, 3.0);
    float3 lightIntensity = float3(20.0, 20.0, 20.0);
    
    float3 ka = float3(0.001, 0.001, 0.001);
    float3 kd = payload.albedo;
    float3 ks = float3(0.7937, 0.7937, 0.7937);
    
    float p = 100.0;
    
    float I = lightIntensity;
    float r2 = dot(lightPos - payload.worldPosition.xyz, lightPos - payload.worldPosition.xyz);
    float3 wi = normalize(lightPos - payload.worldPosition.xyz);
    float3 n = normalize(payload.worldNormal);
    float3 wo = normalize(payload.cameraWorldPosition - payload.worldPosition.xyz);
    float3 h = normalize(wi + wo);
    float3 Ld = float3(0.0, 0.0, 0.0);
    float3 Ls = float3(0.0, 0.0, 0.0);
    float3 La = float3(0.0, 0.0, 0.0);
    Ld = kd * I * max(0.0, dot(n, wi)) / r2;
    Ls = ks * I * pow(max(0.0, dot(n, h)), p) / r2;
    La = ka * I;
    
    float3 color = Ld + Ls + La;
    
    return float4(color, 1.0);
}

PSInput VSMain(VSInput input)
{
    PSInput result;
    
    result.worldPosition = input.position;
    result.worldNormal = input.normal;
    
    float4 ndcPos = input.position;
    float4 ndcNormal = float4(input.normal, 1.0);
    ndcPos = mul(view, ndcPos);
    ndcPos = mul(projection, ndcPos);
    ndcNormal = mul(view, ndcNormal);
    ndcNormal = mul(projection, ndcNormal);
    
    result.ndcPosition = ndcPos;
    result.ndcNormal = ndcNormal;
    result.uv = input.uv;
    result.color = input.color;
    result.cameraWorldPosition = float3(viewInv[0][3], viewInv[1][3], viewInv[2][3]);
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{    
    float4 albedo;
    
    if (input.uv.x == 0 && input.uv.y == 0) // Other objects
    {
        albedo = input.color;
    }
    else
    {
        albedo = t1.Sample(s1, input.uv);
    }
    
    float3 lightPos = float3(3.0, 4.0, -3.0);
    
    BPPayload payload;
    payload.worldPosition = input.worldPosition;
    payload.worldNormal = input.worldNormal;
    payload.cameraWorldPosition = input.cameraWorldPosition;
    payload.ndcPosition = input.ndcPosition;
    payload.ndcNormal = input.ndcNormal;
    payload.albedo = albedo;
    
    //return float4(-view[0][3], -view[1][3], -view[2][3], 1.0);
        
    return BlinnPhong(payload);
}
