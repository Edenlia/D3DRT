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
}

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

struct VSInput
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : UV;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    // #DXR Extra: Perspective Camera
    float4 pos = input.position;
    pos = mul(view, pos);
    pos = mul(projection, pos);
    result.position = pos;
    result.color = input.color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
