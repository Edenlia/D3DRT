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

#include "./DXAPI/stdafx.h"
#include "D3DRTWindow.h"
#include "./Util/D3DUtil.h"
#include "UploadBufferResource.h"
#include "./DXRHelpers/DXRHelper.h"
#include <array>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include "./DXRHelpers/nv_helpers_dx12/RaytracingPipelineGenerator.h"   
#include "./DXRHelpers/nv_helpers_dx12/RootSignatureGenerator.h"
#include "glm/gtc/type_ptr.hpp"
#include "./DXRHelpers/nv_helpers_dx12/manipulator.h"
#include "Windowsx.h"
#include "stb/stb_image.h"
#include "Util/Utility.h"
#include <glm/gtc/matrix_transform.hpp>

D3DRTWindow::D3DRTWindow(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0)
{
}

void D3DRTWindow::OnInit()
{
    nv_helpers_dx12::CameraManip.setWindowSize(GetWidth(), GetHeight());
    nv_helpers_dx12::CameraManip.setLookat(glm::vec3(1.5f, 1.5f, 1.5f), glm::vec3(0, 0, 0),
        glm::vec3(0, 1, 0));
    LoadPipeline();
    LoadAssets();

    InitImGui();

    // Check the raytracing capabilities of the device
    CheckRaytracingSupport();

    // Setup the acceleration structures (AS) for raytracing. When setting up
    // geometry, each bottom-level AS has its own transform matrix.
    CreateAccelerationStructures();

    // Create the raytracing pipeline, associating the shader code to symbol names
    // and to their root signatures, and defining the amount of memory carried by
    // rays (ray payload)
    CreateRaytracingPipeline(); // #DXR

    // #DXR Extra: Per-Instance Data
    // Create a constant buffers, with a color for each vertex of the triangle, for each
    // triangle instance
    //CreateGlobalConstantBuffer();

    // Allocate the buffer storing the raytracing output, with the same dimensions
    // as the target image
    CreateRaytracingOutputBuffer(); // #DXR

    // #DXR Extra: Perspective Camera
    // Create a buffer to store the modelview and perspective camera matrices
    CreateCameraBuffer();
    CreateMaterialBuffer();
    CreateRayTracingGlobalConstantBuffer();

    // Create the buffer containing the raytracing result (always output in a
    // UAV), and create the heap referencing the resources used by the raytracing,
    // such as the acceleration structure
    CreateShaderResourceHeap(); // #DXR

    // Create the shader binding table and indicating which shaders
    // are invoked for each instance in the  AS
    CreateShaderBindingTable(); // #DXR

    // Command lists are created in the recording state, but there is
    // nothing to record yet. The main loop expects it to be closed, so
    // close it now.
    ThrowIfFailed(g_commandList->Close());

}

// Load the rendering pipeline dependencies.
void D3DRTWindow::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&g_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&g_device)
            ));
    }

    // #DXR Extra: Depth Buffering
    // The original sample does not support depth buffering, so we need to allocate a depth buffer,
    // and later bind it before rasterization
    CreateDepthBuffer();

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameNum;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameNum;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameNum; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            g_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    ThrowIfFailed(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator)));
}

// Load the sample assets.
void D3DRTWindow::LoadAssets()
{
    m_renderer = std::make_shared<GraphicsRenderer>();
    m_renderer->Initialize();

    {
        CreateRasterizerDescriptorHeap();
    }

    // Create the command list.
    ThrowIfFailed(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList)));

    // Load all meshes, triangle, plane, menger, dragon
    LoadMeshes();
    ImportTexture();
    CreateFrameResourcesBuffer();

    BuildProceduralGeometryAABBs();

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

// Update frame-based values.
void D3DRTWindow::OnUpdate()
{
    // #DXR Extra: Perspective Camera
    UpdateCameraBuffer();
    UpdateMaterialBuffer();
    UpdateRayTracingGlobalConstantBuffer();
}

// Render the scene.
void D3DRTWindow::OnRender()
{
    RenderImGui();

    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void D3DRTWindow::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void D3DRTWindow::OnKeyUp(UINT8 key)
{
    // Alternate between rasterization and raytracing using the spacebar
    if (key == VK_SPACE)
    {
        m_raster = !m_raster;
    }
}

void D3DRTWindow::OnButtonDown(UINT32 lParam)
{
    bool is_hovered_in_imgui = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (m_raster && is_hovered_in_imgui) {
		return;
	}

    nv_helpers_dx12::CameraManip.setMousePosition(-GET_X_LPARAM(lParam), -GET_Y_LPARAM(lParam));
    m_rayTracingFrameCount = 0; // reset accumulation
}

void D3DRTWindow::OnMouseMove(UINT8 wParam, UINT32 lParam)
{
    bool is_hovered_in_imgui = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (m_raster && is_hovered_in_imgui) {
        return;
    }

    using nv_helpers_dx12::Manipulator;
    Manipulator::Inputs inputs;
    inputs.lmb = wParam & MK_LBUTTON;
    inputs.mmb = wParam & MK_MBUTTON;
    inputs.rmb = wParam & MK_RBUTTON;
    if (!inputs.lmb && !inputs.rmb && !inputs.mmb)
        return; // no mouse button pressed

    inputs.ctrl = GetAsyncKeyState(VK_CONTROL);
    inputs.shift = GetAsyncKeyState(VK_SHIFT);
    inputs.alt = GetAsyncKeyState(VK_MENU);

    CameraManip.mouseMove(-GET_X_LPARAM(lParam), -GET_Y_LPARAM(lParam), inputs);

    m_rayTracingFrameCount = 0; // reset accumulation
}

void D3DRTWindow::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(g_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(g_commandList->Reset(g_commandAllocator.Get(), m_renderer->GetDefaultPSO()));

    // Set necessary state.
    g_commandList->SetGraphicsRootSignature(m_renderer->GetRootSignature());
    g_commandList->RSSetViewports(1, &m_viewport);
    g_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    // #DXR Extra: Depth Buffering
    // Bind the depth buffer as a render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands.
    // #DXR
    if (m_raster) {
        // g_commandList->SetPipelineState(m_basePSO.GetPipelineStateObject());

        // set the constant buffer descriptor heap
        g_commandList->SetGraphicsRootConstantBufferView(0 /*root sig param 0*/, m_cameraBuffer.Get()->GetGPUVirtualAddress()); // camera buffer
        g_commandList->SetGraphicsRootConstantBufferView(3 /*root sig param 3*/, m_materialBuffer.Get()->GetGPUVirtualAddress()); // Disney material params
        // set the root descriptor table 1 to the texture descriptor heap
        std::vector< ID3D12DescriptorHeap* > heaps = { m_rastSrvUavDescHeap.Get() };
        g_commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
        g_commandList->SetGraphicsRootDescriptorTable(
            1, m_rastSrvUavDescHeap->GetGPUDescriptorHandleForHeapStart());

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // #DXR Extra: Depth Buffering
        g_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        m_renderer->Draw(m_planeMeshResource);
        // m_renderer->Draw(m_mengerMeshResource);
        m_renderer->Draw(m_dragonMeshResource);
        m_renderer->Draw(m_armadilloMeshResource);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList.Get());

    }
    else {
        // #DXR
        // Bind the descriptor heap giving access to the top-level acceleration
        // structure, as well as the raytracing output
        std::vector<ID3D12DescriptorHeap*> heaps = { m_srvUavHeap.Get() };
        g_commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()),
            heaps.data());

        // On the last frame, the raytracing output was used as a copy source, to
        // copy its contents into the render target. Now we need to transition it to
        // a UAV so that the shaders can write in it.
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        g_commandList->ResourceBarrier(1, &transition);

        // Setup the raytracing task
        D3D12_DISPATCH_RAYS_DESC desc = {};
        // The layout of the SBT is as follows: ray generation shader, miss
        // shaders, hit groups. As described in the CreateShaderBindingTable method,
        // all SBT entries of a given type have the same size to allow a fixed stride.

        // The ray generation shaders are always at the beginning of the SBT. 
        uint32_t rayGenerationSectionSizeInBytes = m_sbtHelper.GetRayGenSectionSize();
        desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress();
        desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;
        // The miss shaders are in the second SBT section, right after the ray
        // generation shader. We have one miss shader for the camera rays and one
        // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
        // also indicate the stride between the two miss shaders, which is the size
        // of a SBT entry
        uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
        desc.MissShaderTable.StartAddress =
            m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
        desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
        desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();
        // The hit groups section start after the miss shaders. In this sample we
        // have one 1 hit group for the triangle
        uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
        desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() +
                                            rayGenerationSectionSizeInBytes +
                                            missSectionSizeInBytes;
        desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
        desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();
        // Dimensions of the image to render, identical to a kernel launch dimension
        desc.Width = GetWidth();
        desc.Height = GetHeight();
        desc.Depth = 1;
        // Bind the raytracing pipeline
        g_commandList->SetPipelineState1(m_rtStateObject.Get());
        // Dispatch the rays and write to the raytracing output
        g_commandList->DispatchRays(&desc);

        // The raytracing output needs to be copied to the actual render target used
        // for display. For this, we need to transition the raytracing output from a
        // UAV to a copy source, and the render target buffer to a copy destination.
        // We can then do the actual copy, before transitioning the render target
        // buffer into a render target, that will be then used to display the image
        transition = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        g_commandList->ResourceBarrier(1, &transition);
        transition = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);
        g_commandList->ResourceBarrier(1, &transition);

        g_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(),
            m_outputResource.Get());

        UpdateFrameBuffer();

        transition = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        g_commandList->ResourceBarrier(1, &transition);
        m_rayTracingFrameCount++;
    }
    

    // Indicate that the back buffer will now be used to present.
    g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(g_commandList->Close());
}

//-----------------------------------------------------------------------------
//
// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
//
D3DRTWindow::AccelerationStructureBuffers D3DRTWindow::CreateBottomLevelAS(
    std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers, 
    std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

    // Adding all vertex buffers and not transforming their position.
    for (size_t i = 0; i < vVertexBuffers.size(); i++) {
        // for (const auto &buffer : vVertexBuffers) {
        if (i < vIndexBuffers.size() && vIndexBuffers[i].second > 0)
            bottomLevelAS.AddVertexBuffer(vVertexBuffers[i].first.Get(), 0,
                vVertexBuffers[i].second, sizeof(Vertex),
                vIndexBuffers[i].first.Get(), 0,
                vIndexBuffers[i].second, nullptr, 0, true);

        else
            bottomLevelAS.AddVertexBuffer(vVertexBuffers[i].first.Get(), 0,
                vVertexBuffers[i].second, sizeof(Vertex), 0,
                0);
    }

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;
    bottomLevelAS.ComputeASBufferSizes(g_device.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

    // Once the sizes are obtained, the application is responsible for allocating
   // the necessary buffers. Since the entire generation will be done on the GPU,
   // we can directly allocate those on the default heap
    AccelerationStructureBuffers buffers;
    buffers.pScratch = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), scratchSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON,
        nv_helpers_dx12::kDefaultHeapProps);
    buffers.pResult = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), resultSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nv_helpers_dx12::kDefaultHeapProps);

    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    bottomLevelAS.Generate(g_commandList.Get(), buffers.pScratch.Get(),
        buffers.pResult.Get(), false, nullptr);

    return buffers;
}

//-----------------------------------------------------------------------------
//
// Allocate the buffer holding the raytracing output, with the same size as the
// output image
//
void D3DRTWindow::CreateRaytracingOutputBuffer()
{
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.DepthOrArraySize = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB
    // formats cannot be used with UAVs. For accuracy we should convert to sRGB
    // ourselves in the shader
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resDesc.Width = GetWidth();
    resDesc.Height = GetHeight();
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    ThrowIfFailed(g_device->CreateCommittedResource(
        &nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
        IID_PPV_ARGS(&m_outputResource)));
}

//-----------------------------------------------------------------------------
//
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
//
void D3DRTWindow::CreateShaderResourceHeap()
{
    // #DXR Extra: Perspective Camera
    // Create a SRV/UAV/CBV descriptor heap. We need 3 entries - 1 SRV for the TLAS, 1 UAV for the
    // raytracing output and 1 CBV for the camera matrices, 1 CBV for the frame count, 1 SRV for the frame texture
    m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(
        g_device.Get(), 5, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // Get a handle to the heap memory on the CPU side, to be able to write the
    // descriptors directly
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
        m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the UAV. Based on the root signature we created it is the first
    // entry. The Create*View methods write the view information directly into
    // srvHandle
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    g_device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc,
        srvHandle);

    // Add the Top Level AS SRV right after the raytracing output buffer
    srvHandle.ptr += g_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location =
        m_topLevelASBuffers.pResult->GetGPUVirtualAddress();
    // Write the acceleration structure view in the heap
    g_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // #DXR Extra: Perspective Camera
    // Add the constant buffer for the camera after the TLAS
    srvHandle.ptr +=
        g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Describe and create a constant buffer view for the camera
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = m_cameraBufferSize;
    g_device->CreateConstantBufferView(&cbvDesc, srvHandle);

    // Frame count
    srvHandle.ptr +=
        g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Describe and create a constant buffer view for frame count
    D3D12_CONSTANT_BUFFER_VIEW_DESC globalCbvDesc = {};
    globalCbvDesc.BufferLocation = m_rayTracingGlobalConstantBuffer->GetGPUVirtualAddress();
    globalCbvDesc.SizeInBytes = m_rayTracingGlobalConstantBufferSize;
    g_device->CreateConstantBufferView(&globalCbvDesc, srvHandle);

    // Frame texture
    srvHandle.ptr +=
        g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Describe and create a constant buffer view for frame count
    D3D12_SHADER_RESOURCE_VIEW_DESC frameSrvDesc = {};
    frameSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    frameSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    frameSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    frameSrvDesc.Texture2D.MipLevels = 1;
    g_device->CreateShaderResourceView(m_frameBuffer.Get(), &frameSrvDesc, srvHandle);
}

//-----------------------------------------------------------------------------
//
// The Shader Binding Table (SBT) is the cornerstone of the raytracing setup:
// this is where the shader resources are bound to the shaders, in a way that
// can be interpreted by the raytracer on GPU. In terms of layout, the SBT
// contains a series of shader IDs with their resource pointers. The SBT
// contains the ray generation shader, the miss shaders, then the hit groups.
// Using the helper class, those can be specified in arbitrary order.
//
void D3DRTWindow::CreateShaderBindingTable()
{
    // The SBT helper class collects calls to Add*Program.  If called several
    // times, the helper must be emptied before re-adding shaders.
    m_sbtHelper.Reset();

    // The pointer to the beginning of the heap is the only parameter required by
    // shaders without root parameters
    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
        m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();

    // The helper treats both root parameter pointers and heap pointers as void*,
    // while DX12 uses the
    // D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this
    // struct is a UINT64, which then has to be reinterpreted as a pointer.
    auto heapPointer = reinterpret_cast<uint64_t*>(srvUavHeapHandle.ptr);

    // The ray generation only uses heap data
    m_sbtHelper.AddRayGenerationProgram(L"RayGen", { heapPointer });

    // The miss and hit shaders do not access any external resources: instead they
    // communicate their results through the ray payload
    m_sbtHelper.AddMissProgram(L"Miss", {});

    // #DXR Extra - Another ray type
    m_sbtHelper.AddMissProgram(L"ShadowMiss", {});

    // Adding the triangle hit shader
    // #DXR Extra: Per-Instance Data
    // We have 3 triangles, each of which needs to access its own constant buffer
    // as a root parameter in its primary hit shader. The shadow hit only sets a
    // boolean visibility in the payload, and does not require external data
    for (int i = 0; i < 3; ++i) {
        m_sbtHelper.AddHitGroup(
            L"HitGroup", 
            { 
                (void*)(m_rayTracingGlobalConstantBuffer->GetGPUVirtualAddress()),
                (void*)(m_dragonMeshResource->GetVertexBuffer()->GetGPUVirtualAddress()),
                (void*)(m_dragonMeshResource->GetIndexBuffer()->GetGPUVirtualAddress())
            }
        );
        // #DXR Extra - Another ray type
        m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});
    }

    // The plane also uses a constant buffer for its vertex colors
    // #DXR Extra - Another ray type
    m_sbtHelper.AddHitGroup(L"PlaneHitGroup", { (void*)(m_rayTracingGlobalConstantBuffer->GetGPUVirtualAddress()), heapPointer });
    m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});

    // menger sponge fractal
    m_sbtHelper.AddHitGroup(L"HitGroup", {
        (void*)(m_rayTracingGlobalConstantBuffer->GetGPUVirtualAddress()),
        (void*)(m_mengerMeshResource->GetVertexBuffer()->GetGPUVirtualAddress()),
        (void*)(m_mengerMeshResource->GetIndexBuffer()->GetGPUVirtualAddress())
        });


    //// #DXR Extra - Another ray type
    m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});

    
    m_sbtHelper.AddHitGroup(L"ProcedualGeometryHitGroup", { });
    m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});
    


    // Compute the size of the SBT given the number of shaders and their
    // parameters
    uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();

    // Create the SBT on the upload heap. This is required as the helper will use
    // mapping to write the SBT contents. After the SBT compilation it could be
    // copied to the default heap for performance.
    m_sbtStorage = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
    if (!m_sbtStorage) {
        throw std::logic_error("Could not allocate the shader binding table");
    }

    // Compile the SBT from the shader and parameters info
    m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
}

void D3DRTWindow::CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances)
{
    // Gather all the instances into the builder helper
    for (size_t i = 0; i < instances.size(); i++) {
        m_topLevelASGenerator.AddInstance(instances[i].first.Get(),
            instances[i].second, static_cast<UINT>(i),
            static_cast<UINT>(2*i));
    }

    // As for the bottom-level AS, the building the AS requires some scratch space
    // to store temporary data in addition to the actual AS. In the case of the
    // top-level AS, the instance descriptors also need to be stored in GPU
    // memory. This call outputs the memory requirements for each (scratch,
    // results, instance descriptors) so that the application can allocate the
    // corresponding memory
    UINT64 scratchSize, resultSize, instanceDescsSize;

    m_topLevelASGenerator.ComputeASBufferSizes(g_device.Get(), true, &scratchSize,
        &resultSize, &instanceDescsSize);

    // Create the scratch and result buffers. Since the build is all done on GPU,
    // those can be allocated on the default heap
    m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nv_helpers_dx12::kDefaultHeapProps);
    m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nv_helpers_dx12::kDefaultHeapProps);

    // The buffer describing the instances: ID, shader binding information,
    // matrices ... Those will be copied into the buffer by the helper through
    // mapping, so the buffer has to be allocated on the upload heap.
    m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    m_topLevelASGenerator.Generate(g_commandList.Get(),
        m_topLevelASBuffers.pScratch.Get(),
        m_topLevelASBuffers.pResult.Get(),
        m_topLevelASBuffers.pInstanceDesc.Get());
}

void D3DRTWindow::CreateAccelerationStructures()
{
    // Build the bottom AS from the Triangle vertex buffer
    AccelerationStructureBuffers bottomLevelBuffers =
        CreateBottomLevelAS({ {m_dragonMeshResource->GetVertexBuffer().Get(), m_dragonMeshResource->GetVertexCount()}}, {{m_dragonMeshResource->GetIndexBuffer().Get(), m_dragonMeshResource->GetIndexCount()}});

    // #DXR Extra: Per-Instance Data
    AccelerationStructureBuffers planeBottomLevelBuffers =
        CreateBottomLevelAS({ {m_planeMeshResource->GetVertexBuffer().Get(), 6}});

     //#DXR Extra: Indexed Geometry    
     //Build the bottom AS from the Menger Sponge vertex buffer
     //#DXR Extra: Indexed Geometry
     //Build the bottom AS from the Menger Sponge vertex buffer
    AccelerationStructureBuffers mengerBottomLevelBuffers =
        CreateBottomLevelAS({ {m_mengerMeshResource->GetVertexBuffer().Get(), m_mengerMeshResource->GetVertexCount()}},
            { {m_mengerMeshResource->GetIndexBuffer().Get(), m_mengerMeshResource->GetIndexCount()}});

    // AccelerationStructureBuffers sphereBottomLevelBuffers = CreateAABBBottomLevelAS();

    // Just one instance for now
    m_instances = 
    { 
        {bottomLevelBuffers.pResult, XMMatrixScaling(0.008f, 0.008f, 0.008f) * XMMatrixIdentity()},
        {bottomLevelBuffers.pResult, XMMatrixScaling(0.008f, 0.008f, 0.008f) * XMMatrixTranslation(-.6f, 0, 0)},
        {bottomLevelBuffers.pResult, XMMatrixScaling(0.008f, 0.008f, 0.008f) * XMMatrixTranslation(.6f, 0, 0)},
        {planeBottomLevelBuffers.pResult, XMMatrixTranslation(0, 0, 0)},
        {mengerBottomLevelBuffers.pResult, XMMatrixIdentity() /*add merger sponge*/},
    };
    CreateTopLevelAS(m_instances);

    // Flush the command list and wait for it to finish
    g_commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    m_fenceValue++;
    g_commandQueue->Signal(m_fence.Get(), m_fenceValue);

    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    WaitForSingleObject(m_fenceEvent, INFINITE);

    // Once the command list is finished executing, reset it to be reused for
    // rendering
    ThrowIfFailed(
        g_commandList->Reset(g_commandAllocator.Get(), m_renderer->GetDefaultPSO()));

    // Store the AS buffers. The rest of the buffers will be released once we exit
    // the function
    m_bottomLevelAS = bottomLevelBuffers.pResult;
}


void D3DRTWindow::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(g_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3DRTWindow::CheckRaytracingSupport()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    ThrowIfFailed(g_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
        &options5, sizeof(options5)));
    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
        throw std::runtime_error("Raytracing not supported on device");
}

ComPtr<ID3D12RootSignature> D3DRTWindow::CreateProcedualGeometryHitSignature() {
    nv_helpers_dx12::RootSignatureGenerator rsc;
    return rsc.Generate(g_device.Get(), true);
}

//-----------------------------------------------------------------------------
//
// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
//
//
void D3DRTWindow::CreateRaytracingPipeline() {
    nv_helpers_dx12::RayTracingPipelineGenerator pipeline(g_device.Get());

    // The pipeline contains the DXIL code of all the shaders potentially executed
    // during the raytracing process. This section compiles the HLSL code into a
    // set of DXIL libraries. We chose to separate the code in several libraries
    // by semantic (ray generation, hit, miss) for clarity. Any code layout can be
    // used.
    m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Raytracing/RayGen.hlsl");
    m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Raytracing/Miss.hlsl");
    m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Raytracing/Hit.hlsl");
    // #DXR Extra - Another ray type
    m_shadowLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Raytracing/ShadowRay.hlsl");
    m_procedualGeometryLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Raytracing/Procedual.hlsl");


    // In a way similar to DLLs, each library is associated with a number of
    // exported symbols. This
    // has to be done explicitly in the lines below. Note that a single library
    // can contain an arbitrary number of symbols, whose semantic is given in HLSL
    // using the [shader("xxx")] syntax
    pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
    pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
    // #DXR Extra: Per-Instance Data
    pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit", L"PlaneClosestHit" });
    // #DXR Extra - Another ray type
    pipeline.AddLibrary(m_shadowLibrary.Get(), { L"ShadowClosestHit", L"ShadowMiss" });
    // Procedural Geometry
    pipeline.AddLibrary(m_procedualGeometryLibrary.Get(), { L"SphereIntersection", L"SphereClosestHit" });

    // Create signature for each shader type
    m_rayGenSig.Reset(1);

    m_rayGenSig[0].InitAsDescriptorTable(5);
    m_rayGenSig[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0 /*u0*/, 1); // u0, raytracing output
    m_rayGenSig[0].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 /*t0*/, 1); // t0, TLAS
    m_rayGenSig[0].SetTableRange(2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0 /*b0*/, 1); // b0, camera parameters
    m_rayGenSig[0].SetTableRange(3, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1 /*b1*/, 1); // b1, global parameters (frameCount)
    m_rayGenSig[0].SetTableRange(4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 /*t1*/, 1); // t1, frame texture buffer

    m_rayGenSig.Finalize(L"RayGen", D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    m_hitSig.Reset(4);
    m_hitSig[0].InitAsConstantBuffer(0 /*b0*/);
    m_hitSig[1].InitAsBufferSRV(0 /*t0*/); // vertices
    m_hitSig[2].InitAsBufferSRV(1 /*t1*/); // indices
    m_hitSig[3].InitAsDescriptorTable(1);
    m_hitSig[3].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2 /*t2*/, 1, 0, 1 /*2nd slot of the heap*/); // t2, BVH
    m_hitSig.Finalize(L"Hit", D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    m_missSig.Reset(0);
    m_missSig.Finalize(L"Miss", D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    m_shadowSig.Reset(4);
    m_shadowSig[0].InitAsConstantBuffer(0 /*b0*/);
    m_shadowSig[1].InitAsBufferSRV(0 /*t0*/); // vertices
    m_shadowSig[2].InitAsBufferSRV(1 /*t1*/); // indices
    m_shadowSig[3].InitAsDescriptorTable(1);
    m_shadowSig[3].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2 /*t2*/, 1, 0, 1 /*2nd slot of the heap*/); // t2, BVH
    m_shadowSig.Finalize(L"Shadow", D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    // 3 different shaders can be invoked to obtain an intersection: an
    // intersection shader is called
    // when hitting the bounding box of non-triangular geometry. This is beyond
    // the scope of this tutorial. An any-hit shader is called on potential
    // intersections. This shader can, for example, perform alpha-testing and
    // discard some intersections. Finally, the closest-hit program is invoked on
    // the intersection point closest to the ray origin. Those 3 shaders are bound
    // together into a hit group.

    // Note that for triangular geometry the intersection shader is built-in. An
    // empty any-hit shader is also defined by default, so in our simple case each
    // hit group contains only the closest hit shader. Note that since the
    // exported symbols are defined above the shaders can be simply referred to by
    // name.

    // Hit group for the triangles, with a shader simply interpolating vertex
    // colors
    pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
    // #DXR Extra: Per-Instance Data
    pipeline.AddHitGroup(L"PlaneHitGroup", L"PlaneClosestHit");
    // #DXR Extra - Another ray type
    // Hit group for all geometry when hit by a shadow ray
    pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");
    // Procedual Geometry
    pipeline.AddHitGroup(L"ProcedualGeometryHitGroup", L"SphereClosestHit", L"", L"SphereIntersection", D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);


    // The following section associates the root signature to each shader. Note
    // that we can explicitly show that some shaders share the same root signature
    // (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
    // to as hit groups, meaning that the underlying intersection, any-hit and
    // closest-hit shaders share the same root signature.
    // #DXR Extra - Another ray type
    pipeline.AddRootSignatureAssociation(m_rayGenSig.GetSignature(), {L"RayGen"});
    pipeline.AddRootSignatureAssociation(m_missSig.GetSignature(), {L"Miss", L"ShadowMiss"});
    pipeline.AddRootSignatureAssociation(m_hitSig.GetSignature(), { L"HitGroup", L"PlaneHitGroup" });
    pipeline.AddRootSignatureAssociation(m_shadowSig.GetSignature(), {L"ShadowHitGroup"});
    pipeline.AddRootSignatureAssociation(m_procedualGeometrySignature.Get(), { L"ProcedualGeometryHitGroup" });

    // The payload size defines the maximum size of the data carried by the rays,
    // ie. the the data
    // exchanged between shaders, such as the HitInfo structure in the HLSL code.
    // It is important to keep this value as low as possible as a too high value
    // would result in unnecessary memory consumption and cache trashing.
    pipeline.SetMaxPayloadSize(7 * sizeof(float) + sizeof(UINT)); // RGB + distance + normal + depth

    // Upon hitting a surface, DXR can provide several attributes to the hit. In
    // our sample we just use the barycentric coordinates defined by the weights
    // u,v of the last two vertices of the triangle. The actual barycentrics can
    // be obtained using float3 barycentrics = float3(1.f-u-v, u, v);
    pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

    // The raytracing process can shoot rays from existing hit points, resulting
    // in nested TraceRay calls. Our sample code traces only primary rays, which
    // then requires a trace depth of 1. Note that this recursion depth should be
    // kept to a minimum for best performance. Path tracing algorithms can be
    // easily flattened into a simple loop in the ray generation.
    // #DXR Extra - Another ray type
    pipeline.SetMaxRecursionDepth(6);

    // Compile the pipeline for execution on the GPU
    try {
        m_rtStateObject = pipeline.Generate();
    } catch (const std::exception& e) {
        ID3D12Debug* d3dDebug = nullptr;

	}
    

    // Cast the state object into a properties object, allowing to later access
    // the shader pointers by name
    ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
}

//----------------------------------------------------------------------------------
//
// The camera buffer is a constant buffer that stores the transform matrices of
// the camera, for use by both the rasterization and raytracing. This method
// allocates the buffer where the matrices will be copied. For the sake of code
// clarity, it also creates a heap containing only this buffer, to use in the
// rasterization path.
//
// #DXR Extra: Perspective Camera
void D3DRTWindow::CreateCameraBuffer()
{
    uint32_t nbMatrix = 4; // view, perspective, viewInv, perspectiveInv
    m_cameraBufferSize = nbMatrix * sizeof(XMMATRIX);

    // Create the constant buffer for all matrices
    m_cameraBuffer = nv_helpers_dx12::CreateBuffer(
        g_device.Get(), m_cameraBufferSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    // Create a descriptor heap that will be used by the rasterization shaders
    m_cameraHeap = nv_helpers_dx12::CreateDescriptorHeap(
        g_device.Get(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // Describe and create the constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = m_cameraBufferSize;

    // Get a handle to the heap memory on the CPU side, to be able to write the
    // descriptors directly
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
        m_cameraHeap->GetCPUDescriptorHandleForHeapStart();
    g_device->CreateConstantBufferView(&cbvDesc, srvHandle);
}

void D3DRTWindow::UpdateCameraBuffer()
{
    std::vector<XMMATRIX> matrices(4);

    // Initialize the view matrix, ideally this should be based on user
    // interactions The lookat and perspective matrices used for rasterization are
    // defined to transform world-space vertices into a [0,1]x[0,1]x[0,1] camera
    // space
    const glm::mat4& mat = nv_helpers_dx12::CameraManip.getMatrix(); 
    memcpy(&matrices[0].r->m128_f32[0], glm::value_ptr(mat), 16 * sizeof(float));

    glm::vec3 testV = nv_helpers_dx12::CameraManip.getPosition();

    glm::mat4 viewInv = glm::inverse(mat);

    float fovAngleY = 45.0f * XM_PI / 180.0f;
    matrices[1] = 
        XMMatrixPerspectiveFovRH(fovAngleY, m_aspectRatio, 0.1f, 1000.0f);

    // Raytracing has to do the contrary of rasterization: rays are defined in
    // camera space, and are transformed into world space. To do this, we need to
    // store the inverse matrices as well.
    XMVECTOR det;
    matrices[2] = XMMatrixInverse(&det, matrices[0]);
    matrices[3] = XMMatrixInverse(&det, matrices[1]);

    // Copy the matrix contents
    uint8_t* pData;
    ThrowIfFailed(m_cameraBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, matrices.data(), m_cameraBufferSize);
    m_cameraBuffer->Unmap(0, nullptr);
}

void D3DRTWindow::CreateMaterialBuffer()
{
    // align to 256 bytes for the descriptor heap
    m_materialBufferSize = (sizeof(DisneyMaterialParams) + 255) & ~255;

    // Create the constant buffer for all matrices
    m_materialBuffer = nv_helpers_dx12::CreateBuffer(
		g_device.Get(), m_materialBufferSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// Create a descriptor heap that will be used by the rasterization shaders
    m_materialHeap = nv_helpers_dx12::CreateDescriptorHeap(
		g_device.Get(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Describe and create the constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_materialBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_materialBufferSize;

	// Get a handle to the heap memory on the CPU side, to be able to write the
	// descriptors directly
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
		m_materialHeap->GetCPUDescriptorHandleForHeapStart();
	g_device->CreateConstantBufferView(&cbvDesc, srvHandle);

    DisneyMaterialParams params = {};
    params.baseColor = XMFLOAT3(0.82, 0.67, 0.16);
    params.metallic = 0.9f;
    params.subsurface = 1.0f;
    params.specular = 1.0f;
    params.roughness = 0.0f;
    params.specularTint = 0.0f;
    params.anisotropic = 0.0f;
    params.sheen = 1.0f;
    params.sheenTint = 0.5f;
    params.clearcoat = 0.0f;
    params.clearcoatGloss = 0.0f;

    // Copy material data
    uint8_t* pData;
    ThrowIfFailed(m_materialBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &params, m_materialBufferSize);
    m_materialBuffer->Unmap(0, nullptr);
}

void D3DRTWindow::UpdateMaterialBuffer()
{
    DisneyMaterialParams params = {};
    params.baseColor = XMFLOAT3(m_imGuiParams.baseColor.x, m_imGuiParams.baseColor.y, m_imGuiParams.baseColor.z);
    params.metallic = m_imGuiParams.metallic;
    params.subsurface = m_imGuiParams.subsurface;
    params.specular = m_imGuiParams.specular;
    params.roughness = m_imGuiParams.roughness;
    params.specularTint = m_imGuiParams.specularTint;
    params.anisotropic = m_imGuiParams.anisotropic;
    params.sheen = m_imGuiParams.sheen;
    params.sheenTint = m_imGuiParams.sheenTint;
    params.clearcoat = m_imGuiParams.clearcoat;
    params.clearcoatGloss = m_imGuiParams.clearcoatGloss;

    // Copy material data
    uint8_t* pData;
    ThrowIfFailed(m_materialBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &params, m_materialBufferSize);
    m_materialBuffer->Unmap(0, nullptr);
}

void D3DRTWindow::CreateRayTracingGlobalConstantBuffer()
{
	// align to 256 bytes for the descriptor heap
	m_rayTracingGlobalConstantBufferSize = (sizeof(RayTracingGlobalParams) + 255) & ~255;

	// Create the constant buffer for all matrices
    m_rayTracingGlobalConstantBuffer = nv_helpers_dx12::CreateBuffer(
		g_device.Get(), m_rayTracingGlobalConstantBufferSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// Create a descriptor heap that will be used by the ray tracing shaders
    m_rayTracingGlobalConstantHeap = nv_helpers_dx12::CreateDescriptorHeap(
		g_device.Get(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Describe and create the constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_rayTracingGlobalConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_rayTracingGlobalConstantBufferSize;

	// Get a handle to the heap memory on the CPU side, to be able to write the
	// descriptors directly
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
		m_rayTracingGlobalConstantHeap->GetCPUDescriptorHandleForHeapStart();
	g_device->CreateConstantBufferView(&cbvDesc, srvHandle);

    RayTracingGlobalParams params = {};
	params.frameCount = 0;

	// Copy material data
	uint8_t* pData;
	ThrowIfFailed(m_rayTracingGlobalConstantBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, &params, m_rayTracingGlobalConstantBufferSize);
	m_rayTracingGlobalConstantBuffer->Unmap(0, nullptr);

}

void D3DRTWindow::UpdateRayTracingGlobalConstantBuffer()
{
    RayTracingGlobalParams params = {};
	params.frameCount = m_rayTracingFrameCount;

	// Copy material data
	uint8_t* pData;
	ThrowIfFailed(m_rayTracingGlobalConstantBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, &params, m_rayTracingGlobalConstantBufferSize);
	m_rayTracingGlobalConstantBuffer->Unmap(0, nullptr);
}

void D3DRTWindow::UpdateFrameBuffer()
{
    g_commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_frameBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST));

    g_commandList->CopyResource(m_frameBuffer.Get(), m_outputResource.Get());

    g_commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_frameBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_COMMON));
}

void D3DRTWindow::CreateRasterizerDescriptorHeap()
{
    // Create descriptor heap for default heap buffer
    D3D12_DESCRIPTOR_HEAP_DESC rasterizerDescHeapDesc = {};
    rasterizerDescHeapDesc.NumDescriptors = 2; // 1 is the texture, 2 is for imgui, 3 is for test raytracing output
    rasterizerDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    rasterizerDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ThrowIfFailed(g_device->CreateDescriptorHeap(&rasterizerDescHeapDesc, IID_PPV_ARGS(&m_rastSrvUavDescHeap)));
}

void D3DRTWindow::InitImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    m_imGuiIO = &ImGui::GetIO();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_rastSrvUavDescHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_rastSrvUavDescHeap->GetGPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    gpuHandle.ptr += g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(Win32Application::GetHwnd());
    ImGui_ImplDX12_Init(g_device.Get(), 2,
        DXGI_FORMAT_R8G8B8A8_UNORM, m_rastSrvUavDescHeap.Get(),
        cpuHandle,
        gpuHandle);

    m_imGuiParams = {};
    m_imGuiParams.baseColor = ImVec4(0.82, 0.67, 0.16, 1);
    m_imGuiParams.metallic = 0.9f;
    m_imGuiParams.subsurface = 1.0f;
    m_imGuiParams.specular = 1.0f;
    m_imGuiParams.roughness = 0.0f;
    m_imGuiParams.specularTint = 0.0f;
    m_imGuiParams.anisotropic = 0.0f;
    m_imGuiParams.sheen = 1.0f;
    m_imGuiParams.sheenTint = 0.5f;
    m_imGuiParams.clearcoat = 0.0f;
    m_imGuiParams.clearcoatGloss = 0.0f;

}

void D3DRTWindow::RenderImGui()
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {

        ImGui::Begin("Material");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("adjust material params.");               // Display some text (you can use a format strings too)

        
        ImGui::ColorEdit3("base color", (float*)&m_imGuiParams.baseColor); // Edit 3 floats representing a color
        ImGui::SliderFloat("metallic", &m_imGuiParams.metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("subsurface", &m_imGuiParams.subsurface, 0.0f, 1.0f);
        ImGui::SliderFloat("specular", &m_imGuiParams.specular, 0.0f, 1.0f);
        ImGui::SliderFloat("roughness", &m_imGuiParams.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("specularTint", &m_imGuiParams.specularTint, 0.0f, 1.0f);
        ImGui::SliderFloat("anisotropic", &m_imGuiParams.anisotropic, 0.0f, 1.0f);
        ImGui::SliderFloat("sheen", &m_imGuiParams.sheen, 0.0f, 1.0f);
        ImGui::SliderFloat("sheenTint", &m_imGuiParams.sheenTint, 0.0f, 1.0f);
        ImGui::SliderFloat("clearcoat", &m_imGuiParams.clearcoat, 0.0f, 1.0f);
        ImGui::SliderFloat("clearcoatGloss", &m_imGuiParams.clearcoatGloss, 0.0f, 1.0f);


        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / m_imGuiIO->Framerate, m_imGuiIO->Framerate);
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
}

void D3DRTWindow::LoadMeshes()
{
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;

    // Create triangle mesh
    ModelLoader::CreateTetrahedron(vertices, indices);
    std::shared_ptr<Mesh> triangleMesh = std::make_shared<Mesh>(vertices, indices);
    m_triangleMeshResource = std::make_shared<MeshResource>(triangleMesh, "triangle");
    m_triangleMeshResource->UploadResource();

    vertices.clear();
    indices.clear();

    // Create plane mesh
    ModelLoader::CreatePlane(vertices);
    std::shared_ptr<Mesh> planeMesh = std::make_shared<Mesh>(vertices);
    m_planeMeshResource = std::make_shared<MeshResource>(planeMesh, "plane");
    m_planeMeshResource->UploadResource();

    vertices.clear();
    indices.clear();

    // Create menger mesh
    nv_helpers_dx12::GenerateMengerSponge(3, 0.75, vertices, indices);
    std::shared_ptr<Mesh> mengerMesh = std::make_shared<Mesh>(vertices, indices);
    m_mengerMeshResource = std::make_shared<MeshResource>(mengerMesh, "menger");
    m_mengerMeshResource->UploadResource();

    vertices.clear();
    indices.clear();

    // Load the dragon model
    ModelLoader::LoadModel("Models/stanford-dragon-pbr/model.dae", vertices, indices);
    std::shared_ptr<Mesh> dragonMesh = std::make_shared<Mesh>(vertices, indices);
    XMMATRIX dragonTransform = XMMatrixScaling(0.008f, 0.008f, 0.008f) * XMMatrixTranslation(1, 0, 0);
    std::shared_ptr<IMaterial> dragonMaterial = std::make_shared<PhongMaterial>();
    m_dragonMeshResource = std::make_shared<MeshResource>(dragonMesh, "dragon", dragonMaterial, dragonTransform);
    m_dragonMeshResource->UploadResource();

    vertices.clear();
    indices.clear();

    // Load the armadillo model
    ModelLoader::LoadModel("Models/stanford-armadillo-pbr/model.dae", vertices, indices);
    std::shared_ptr<Mesh> armadilloMesh = std::make_shared<Mesh>(vertices, indices);
    XMMATRIX armadilloTransform = XMMatrixScaling(0.008f, 0.008f, 0.008f) * XMMatrixTranslation(0, 0, 0);
    std::shared_ptr<IMaterial> armadilloMaterial = std::make_shared<DisneyMaterial>();
    m_armadilloMeshResource = std::make_shared<MeshResource>(armadilloMesh, "armadillo", armadilloMaterial, armadilloTransform);
    m_armadilloMeshResource->UploadResource();

}

//-----------------------------------------------------------------------------
//
// Create the depth buffer for rasterization. This buffer needs to be kept in a separate heap
// 
// #DXR Extra: Depth Buffering
void D3DRTWindow::CreateDepthBuffer()
{
    // The depth buffer heap type is specific for that usage, and the heap contents are not visible
  // from the shaders
    m_dsvHeap = nv_helpers_dx12::CreateDescriptorHeap(g_device.Get(), 1,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

    // The depth and stencil can be packed into a single 32-bit texture buffer. Since we do not need
    // stencil, we use the 32 bits to store depth information (DXGI_FORMAT_D32_FLOAT).
    D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC depthResourceDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 1);
    depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // The depth values will be initialized to 1
    CD3DX12_CLEAR_VALUE depthOptimizedClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

    // Allocate the buffer itself, with a state allowing depth writes
    ThrowIfFailed(g_device->CreateCommittedResource(
        &depthHeapProperties, D3D12_HEAP_FLAG_NONE, &depthResourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&m_depthStencil)));

    // Write the depth buffer view into the depth buffer heap
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    g_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3DRTWindow::BuildProceduralGeometryAABBs()
{
    // #DXR Extra: Procedural Geometry
    auto InitializeAABB = [&](auto& basePosition, auto& size)
        {
            return D3D12_RAYTRACING_AABB{
                basePosition.x - size.x, 
                basePosition.y - size.y, 
                basePosition.z - size.z,
                basePosition.x + size.x, 
                basePosition.y + size.y, 
                basePosition.z + size.z
            };
        };

    m_aabbs.resize(1);

    m_aabbs[0] = InitializeAABB(XMINT3(0, 0, 0), XMFLOAT3(3, 3, 3));

    UINT64 aabbSize = m_aabbs.size() * sizeof(m_aabbs[0]);

    // Store aabb in upload heap
    CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(aabbSize);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &heapProperty, 
        D3D12_HEAP_FLAG_NONE, 
        &bufferResource, //
        D3D12_RESOURCE_STATE_GENERIC_READ, 
        nullptr, 
        IID_PPV_ARGS(&m_aabbBuffer)));

    m_aabbBuffer->SetName(L"AABB Buffer");

    // Copy the aabb data to the index buffer.
    UINT8* pAABBDataBegin;
    CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_aabbBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pAABBDataBegin)));
    memcpy(pAABBDataBegin, m_aabbs.data(), aabbSize);
    m_aabbBuffer->Unmap(0, nullptr);
}

D3DRTWindow::AccelerationStructureBuffers D3DRTWindow::CreateAABBBottomLevelAS() {
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(m_aabbs.size());
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> bottomLevelAS;

    // build geometry desc
    D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_RAYTRACING_GEOMETRY_DESC aabbDescTemplate = {};
    aabbDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
    aabbDescTemplate.AABBs.AABBCount = 1;
    aabbDescTemplate.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
    aabbDescTemplate.Flags = geometryFlags;

    for (int i = 0; i < geometryDescs.size(); ++i)
    {
		geometryDescs[i] = aabbDescTemplate;
		geometryDescs[i].AABBs.AABBs.StartAddress = m_aabbBuffer->GetGPUVirtualAddress() + i * sizeof(D3D12_RAYTRACING_AABB);
	}

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;

    bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    bottomLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE; // Create fast trace BVH
    bottomLevelInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
    bottomLevelInputs.pGeometryDescs = geometryDescs.data();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    g_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    ThrowIfFailed(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    // Create a scratch buffer.
    AllocateUAVBuffer(g_device.Get(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    // Allocate resources for acceleration structures.
    // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
    // Default heap is OK since the application doesn’t need CPU read/write access to them. 
    // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
    // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
    //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
    //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
    {
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(g_device.Get(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState, L"BottomLevelAccelerationStructure");
    }

    // bottom-level AS desc.
    {
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->GetGPUVirtualAddress();
    }

    // Build the acceleration structure.
    g_commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

    AccelerationStructureBuffers bottomLevelASBuffers;
    bottomLevelASBuffers.pResult = bottomLevelAS;
    bottomLevelASBuffers.pScratch = scratch;
    bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;

    return bottomLevelASBuffers;
}


void D3DRTWindow::ImportTexture() {
    // Create Texture
    int width, height, channels;

    // Read as 4 channel
    unsigned char* imageData = stbi_load("Models/stanford-dragon-pbr/textures/DefaultMaterial_albedo.jpg", &width, &height, &channels, 4);

    channels = 4;

    if (imageData) {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Alignment = 0;
        textureDesc.Width = 1280;
        textureDesc.Height = 720;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create default heap, as upload heap's transmission target
        ThrowIfFailed(g_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COMMON,// default heap is copy destination, so init as common state
            nullptr,
            IID_PPV_ARGS(&m_textureBuffer)));

        m_textureBuffer->SetName(L"Texture");

        // Use GetCopyableFootprints to get the layout of the texture should be in upload heap
        // Default heap 
        // Manual calculate the size of the texture is also ok, but GetCopyableFootprints is more convenient
        UINT64  textureUploadBufferSize;
        //textureHeapSize  = ((((width * 4) + 255) & ~255) * (height - 1)) + (width * 4);
        g_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

        // Create upload heap, write cpu memory data and send it to defalut heap
        ThrowIfFailed(g_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_textureUploadBuffer)));

        m_textureUploadBuffer->SetName(L"Texture Upload buffer");

        // Set resource state from common to copy destination (default heap is the receive target)
        g_commandList->ResourceBarrier(1,
            &CD3DX12_RESOURCE_BARRIER::Transition(m_textureBuffer.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST));

        // store vertex buffer in upload heap
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = imageData; // pointer to our image data
        textureData.RowPitch = textureDesc.Width * channels; // size of a row in the texture data
        textureData.SlicePitch = textureData.RowPitch * textureDesc.Height; // size of entire texture data

        // Now we copy the upload buffer contents to the default heap
        UpdateSubresources(g_commandList.Get(), m_textureBuffer.Get(), m_textureUploadBuffer.Get(), 0, 0, 1, &textureData);

        // Set resource state from copy destination to generic read (only shader can access to it)
        g_commandList->ResourceBarrier(1,
            &CD3DX12_RESOURCE_BARRIER::Transition(m_textureBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Create SRV based on default heap buffer
        D3D12_CPU_DESCRIPTOR_HANDLE textureDescHeapHandle = m_rastSrvUavDescHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        g_device->CreateShaderResourceView(m_textureBuffer.Get(), &srvDesc, textureDescHeapHandle);
    }
}

void D3DRTWindow::CreateFrameResourcesBuffer()
{
    D3D12_RESOURCE_DESC frameDesc = {};
    frameDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    frameDesc.Alignment = 0;
    frameDesc.Width = GetWidth();
    frameDesc.Height = GetHeight();
    frameDesc.DepthOrArraySize = 1;
    frameDesc.MipLevels = 1;
    frameDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    frameDesc.SampleDesc.Count = 1;
    frameDesc.SampleDesc.Quality = 0;
    frameDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    frameDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(g_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &frameDesc,
        D3D12_RESOURCE_STATE_COMMON,// default heap is copy destination, so init as common state
        nullptr,
        IID_PPV_ARGS(&m_frameBuffer)));

    m_frameBuffer->SetName(L"Last Frame");
}
