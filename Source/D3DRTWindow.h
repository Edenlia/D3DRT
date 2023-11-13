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
#include "DXRHelpers/nv_helpers_dx12/TopLevelASGenerator.h"
#include "DXRHelpers/nv_helpers_dx12/BottomLevelASGenerator.h"
#include "DXRHelpers/nv_helpers_dx12/ShaderBindingTableGenerator.h"
#include "GraphicsCore.h"
#include "Meshes/Mesh.h"
#include "Meshes/MeshResource.h"
#include "ModelLoader.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "Render/Renderer.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;
using Graphics::g_device;
using Graphics::g_commandAllocator;
using Graphics::g_commandQueue;
using Graphics::g_commandList;

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

struct ImGuiParams {
    ImVec4 baseColor;
    float metallic;
    float subsurface;
    float specular;
    float roughness;
    float specularTint;
    float anisotropic;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatGloss;
};


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

private:
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
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize;
    
    std::shared_ptr<MeshResource> m_triangleMeshResource;
    std::shared_ptr<MeshResource> m_planeMeshResource;
    std::shared_ptr<MeshResource> m_mengerMeshResource;
    std::shared_ptr<MeshResource> m_dragonMeshResource;
    std::shared_ptr<MeshResource> m_armadilloMeshResource;

    std::shared_ptr<IRenderer> m_renderer;

    void ImportTexture();

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
    ComPtr< ID3D12DescriptorHeap > m_cameraHeap;
    uint32_t m_cameraBufferSize = 0;

    // Disney material parameters
    ComPtr< ID3D12Resource > m_materialBuffer;
    ComPtr< ID3D12DescriptorHeap > m_materialHeap;
    uint32_t m_materialBufferSize = 0;

    // #DXR Extra: Per-Instance Data
    ComPtr<ID3D12Resource> m_globalConstantBuffer;
    std::vector<ComPtr<ID3D12Resource>> m_perInstanceConstantBuffers;

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

    ComPtr<ID3D12Resource> m_textureUploadBuffer;
    ComPtr<ID3D12Resource> m_textureBuffer;
    ComPtr<ID3D12DescriptorHeap> m_rastSrvUavDescHeap;

    ImGuiIO* m_imGuiIO;

    ImGuiParams m_imGuiParams;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    bool m_raster = false;

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

    void CreateMaterialBuffer();
    void UpdateMaterialBuffer();

    void CreateRasterizerDescriptorHeap();

    void InitImGui();
    void RenderImGui();

    void LoadMeshes();

    // #DXR Extra: Per-Instance Data
    void CreatePerInstanceConstantBuffers();

    // #DXR Extra: Depth Buffering
    void CreateDepthBuffer();

    // Procedural Geometry
    void BuildProceduralGeometryAABBs();
    D3DRTWindow::AccelerationStructureBuffers D3DRTWindow::CreateAABBBottomLevelAS();
    ComPtr<ID3D12RootSignature> D3DRTWindow::CreateProcedualGeometryHitSignature();
};

#endif // !_D3DRT_WINDOWS_H_
