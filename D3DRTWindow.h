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

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3DRTWindow : public DXSample
{
public:
    D3DRTWindow(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyUp(UINT8 key);

    ComPtr<ID3D12Resource> CreateDefaultBuffer(
		const void* const initData,
		const UINT64 byteSize,
		ComPtr<ID3D12Resource>& uploadBuffer);

private:
    struct Vertex
    {
        XMFLOAT3 POSITION;
        XMFLOAT4 COLOR;
    };

    // #DXR
    struct AccelerationStructureBuffers
    {
        ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
        ComPtr<ID3D12Resource> pResult;       // Where the AS is
        ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
    };

    static const UINT FrameCount = 2;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device5> m_device;
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

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void CheckRaytracingSupport();
    D3DRTWindow::AccelerationStructureBuffers D3DRTWindow::CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers);
    void CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& bottomLevelASInstances);
    void CreateAccelerationStructures();
};

#endif // !_D3DRT_WINDOWS_H_
