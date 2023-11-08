#pragma once

#include "DXAPI/stdafx.h"
#include "Meshes/Mesh.h"
#include "GraphicsCore.h"

#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
using Graphics::g_device;
using Graphics::g_commandAllocator;
using Graphics::g_commandQueue;
using Graphics::g_commandList;

class MeshResource
{
public:
	MeshResource(std::shared_ptr<Mesh> mesh, const std::string& name) : m_mesh(mesh) {
		m_name = name;
	};
	~MeshResource() {};

	bool isUploaded() const { return m_uploaded; }

	const std::string& GetName() const { return m_name; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return m_vertexBufferView; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return m_indexBufferView; }

	const UINT GetVertexCount() const { return m_mesh->GetVertexCount(); }
	const UINT GetIndexCount() const { return m_mesh->GetIndexCount(); }

	void UploadResource();

private:
	ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer);

	bool m_uploaded = false;
	std::shared_ptr<Mesh> m_mesh;
	ComPtr<ID3D12Resource> m_vertexUploadBuffer;
	ComPtr<ID3D12Resource> m_indexUploadBuffer;
	ComPtr<ID3D12Resource> m_vertexBuffer; // Vertex Buffer stored on GPU (Default Heap)
	ComPtr<ID3D12Resource> m_indexBuffer; // Index Buffer stored on GPU (Default Heap)
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	std::string m_name;
	std::shared_ptr<IMaterial> m_material;
};

