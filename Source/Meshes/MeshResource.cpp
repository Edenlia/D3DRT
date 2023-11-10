#include "MeshResource.h"
#include "Util/Utility.h"

void MeshResource::UploadResource()
{
    {
        const UINT modelVBSize = static_cast<UINT>(m_mesh->GetVertexCount()) * sizeof(Vertex);

        m_vertexBuffer = CreateDefaultBuffer(m_mesh->GetVertices().data(), modelVBSize, m_vertexUploadBuffer);

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = modelVBSize;
    }

    
    if (!m_mesh->IsVerticeOnly()) { // If the mesh is vertice only, then it doesn't have index buffer
        const UINT modelIBSize = static_cast<UINT>(m_mesh->GetIndexCount()) * sizeof(UINT);

        m_indexBuffer = CreateDefaultBuffer(m_mesh->GetIndices().data(), modelIBSize, m_indexUploadBuffer);

        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = modelIBSize;
    }

    {
        const UINT modelCBSize = sizeof(XMMATRIX);
        CreateUploadBuffer(&m_worldMatrix, modelCBSize, m_worldMatrixBuffer);
    }

    m_uploaded = true;
}

void MeshResource::UpdateWorldBuffer()
{
    // Copy the matrix contents

    const UINT modelCBSize = sizeof(XMMATRIX);

    uint8_t* pData;
    ThrowIfFailed(m_worldMatrixBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &m_worldMatrix, modelCBSize);
    m_worldMatrixBuffer->Unmap(0, nullptr);
}

ComPtr<ID3D12Resource> MeshResource::CreateDefaultBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer)
{
    // Create upload heap, write cpu memory data and send it to defalut heap
    ThrowIfFailed(g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)));

    ComPtr<ID3D12Resource> defaultBuffer;
    // Create default heap, as upload heap's transmission target
    ThrowIfFailed(g_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_COMMON,// default heap is copy destination, so init as common state
        nullptr,
        IID_PPV_ARGS(&defaultBuffer)));

    // Set resource state from common to copy destination (default heap is the receive target)
    g_commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST));

    // Copy data from cpu memory to default heap
    D3D12_SUBRESOURCE_DATA subResourceData;
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;
    // Core function UpdateSubresources: copy data from cpu memory to upload heap, then copy data from upload heap to default heap
    // The sixth parameter is the max number of subresources to be copied  (Defined in template, means have 2 subresources).
    UpdateSubresources<1>(g_commandList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

    // Set resource state from copy destination to generic read (only shader can access to it)
    g_commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ));

    return defaultBuffer;
}

void MeshResource::CreateUploadBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& buffer)
{
    // Create upload heap, write cpu memory data and send it to defalut heap
    ThrowIfFailed(g_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer)));

    // Copy the matrix contents
    uint8_t* pData;
    ThrowIfFailed(m_worldMatrixBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, initData, byteSize);
    buffer->Unmap(0, nullptr);
}


