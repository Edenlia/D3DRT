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

#ifndef _D3DRT_WINDOWS_H_
#define _D3DRT_WINDOWS_H_

#include "DXSample.h"
#include <dxcapi.h>
#include <vector>
#include "./DXRHelpers/nv_helpers_dx12/TopLevelASGenerator.h"
#include "./DXRHelpers/nv_helpers_dx12/BottomLevelASGenerator.h"
#include "./DXRHelpers/nv_helpers_dx12/ShaderBindingTableGenerator.h"
#include "GraphicsCore.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;
using Graphics::g_device;

inline void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON, const wchar_t* resourceName = nullptr)
{
    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        initialResourceState,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
}


class D3DRTWindow : public DXSample
{
public:
    D3DRTWindow(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyUp(UINT8 key);

    // #DXR Extra: Perspective Camera++
    void OnButtonDown(UINT32 lParam);
    void OnMouseMove(UINT8 wParam, UINT32 lParam);

    ComPtr<ID3D12Resource> CreateDefaultBuffer(
		const void* const initData,
		const UINT64 byteSize,
		ComPtr<ID3D12Resource>& uploadBuffer);

private:
    struct Vertex
    {
        // #DXR Extra: Indexed Geometry
        Vertex(XMFLOAT4 pos, XMFLOAT4 /*n*/, XMFLOAT4 col)
            :POSITION(pos.x, pos.y, pos.z), COLOR(col)
        {}
        Vertex(XMFLOAT3 pos, XMFLOAT4 col)
            :POSITION(pos), COLOR(col)
        {}

        XMFLOAT3 POSITION;
        XMFLOAT4 COLOR;
    };

    // #DXR
    struct AccelerationStructureBuffers
    {
        ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
        ComPtr<ID3D12Resource> pResult;       // Where the AS is
        ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
        UINT64                 ResultDataMaxSizeInBytes = 0;
    };

    static const UINT FrameCount = 2;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;
    UINT m_rtvDescriptorSize;

    // App resources.
    ComPtr<ID3D12Resource> m_vertexUploadBuffer;
    ComPtr<ID3D12Resource> m_vertexDefaultBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3DBlob> m_vertexCpuBuffer;
    ComPtr<ID3D12Resource> m_indexUploadBuffer;
    ComPtr<ID3D12Resource> m_indexDefaultBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    ComPtr<ID3DBlob> m_indexCpuBuffer;

    // #DXR
    ComPtr<IDxcBlob> m_rayGenLibrary;
    ComPtr<IDxcBlob> m_hitLibrary;
    ComPtr<IDxcBlob> m_missLibrary;
    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;
    ComPtr<ID3D12Resource> m_outputResource;
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

    // #DXR Extra: Perspective Camera
    ComPtr< ID3D12Resource > m_cameraBuffer;
    ComPtr< ID3D12DescriptorHeap > m_constHeap;
    uint32_t m_cameraBufferSize = 0;

    // #DXR Extra: Per-Instance Data
    ComPtr<ID3D12Resource> m_planeBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_planeBufferView;
    ComPtr<ID3D12Resource> m_globalConstantBuffer;
    std::vector<ComPtr<ID3D12Resource>> m_perInstanceConstantBuffers;

    // #DXR Extra: Indexed Geometry
    ComPtr<ID3D12Resource> m_mengerVB;
    ComPtr<ID3D12Resource> m_mengerIB;
    D3D12_VERTEX_BUFFER_VIEW m_mengerVBView;
    D3D12_INDEX_BUFFER_VIEW m_mengerIBView;

    UINT m_mengerIndexCount;
    UINT m_mengerVertexCount;

    // #DXR Extra: Depth Buffering
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthStencil;

    // #DXR Extra - Another ray type
    ComPtr<IDxcBlob> m_shadowLibrary;
    ComPtr<ID3D12RootSignature> m_shadowSignature;

    // Procedural Geometry
    ComPtr<ID3D12Resource> m_aabbBuffer;
    std::vector<D3D12_RAYTRACING_AABB> m_aabbs;
    ComPtr<IDxcBlob> m_procedualGeometryLibrary;
    ComPtr<ID3D12RootSignature> m_procedualGeometrySignature;

    // Ray tracing pipeline state
    ComPtr<ID3D12StateObject> m_rtStateObject;
    // Ray tracing pipeline state properties, retaining the shader identifiers
    // to use in the Shader Binding Table
    ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;


    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    bool m_raster = true;

    // #DXR acceleration structure
    ComPtr<ID3D12Resource> m_bottomLevelAS; // Storage for the bottom Level AS
    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator; // Helper to generate all the steps required to build a TLAS
    AccelerationStructureBuffers m_topLevelASBuffers; // Scratch buffers for the top Level AS
    std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> m_instances;

    // #DXR shader table
    nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
    ComPtr<ID3D12Resource> m_sbtStorage;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void CheckRaytracingSupport();
    D3DRTWindow::AccelerationStructureBuffers D3DRTWindow::CreateBottomLevelAS(
        std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers,
        std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers = {}
    );
    void CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& bottomLevelASInstances);
    void CreateAccelerationStructures();

    // #DXR
    ComPtr<ID3D12RootSignature> CreateRayGenSignature();
    ComPtr<ID3D12RootSignature> CreateHitSignature();
    ComPtr<ID3D12RootSignature> CreateMissSignature();
    void CreateRaytracingOutputBuffer();
    void CreateShaderResourceHeap();
    void CreateShaderBindingTable();
    
    void CreateRaytracingPipeline();

    // #DXR Extra: Perspective Camera
    void CreateCameraBuffer();
    void UpdateCameraBuffer();

    // #DXR Extra: Per-Instance Data
    void CreatePlaneVB();
    void CreateGlobalConstantBuffer();
    void CreatePerInstanceConstantBuffers();

    // #DXR Extra: Indexed Geometry
    void CreateMengerSpongeVB();

    // #DXR Extra: Depth Buffering
    void CreateDepthBuffer();

    // Procedural Geometry
    void BuildProceduralGeometryAABBs();
    D3DRTWindow::AccelerationStructureBuffers D3DRTWindow::CreateAABBBottomLevelAS();
    ComPtr<ID3D12RootSignature> D3DRTWindow::CreateProcedualGeometryHitSignature();
};

#endif // !_D3DRT_WINDOWS_H_
