#pragma once

#include "DXAPI/stdafx.h"
#include "Meshes/Mesh.h"
#include "GraphicsCore.h"

#include <memory>
#include <string>

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using Graphics::g_device;
using Graphics::g_commandAllocator;
using Graphics::g_commandQueue;
using Graphics::g_commandList;

class MeshResource
{
public:
	MeshResource(
		std::shared_ptr<Mesh> mesh, 
		const std::string& name, 
		std::shared_ptr<IMaterialResource> material = std::make_shared<BaseMaterialResource>(),
		const XMMATRIX& worldMatrix = XMMatrixIdentity()
	) : m_mesh(mesh) {
		m_name = name;
		m_worldMatrix = worldMatrix;
		m_material = material;

	};

	~MeshResource() {};

	bool isUploaded() const { return m_uploaded; }

	const std::string& GetName() const { return m_name; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return m_vertexBufferView; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return m_indexBufferView; }
	const ComPtr<ID3D12Resource>& GetVertexBuffer() const { return m_vertexBuffer; }
	const ComPtr<ID3D12Resource>& GetIndexBuffer() const { return m_indexBuffer; }
	const ComPtr<ID3D12Resource>& GetWorldMatrixBuffer() const { return m_worldMatrixBuffer; }
	const XMMATRIX& GetWorldMatrix() const { return m_worldMatrix; }
	const std::shared_ptr<IMaterialResource>& GetMaterial() const { return m_material; }

	const UINT GetVertexCount() const { return m_mesh->GetVertexCount(); }
	const UINT GetIndexCount() const { return m_mesh->GetIndexCount(); }
	const bool IsVerticeOnly() const { return m_mesh->IsVerticeOnly(); }

	void SetWorldMatrix(const XMMATRIX& worldMatrix) { 
		m_worldMatrix = worldMatrix; 
		UpdateWorldBuffer();
	}

	void SetMaterialParam(const IMaterialParams& params) { m_material->SetMaterialParams(params); }

	void UploadResource();
	
private:
	ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer);
	void CreateUploadBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& buffer);
	void UpdateWorldBuffer();

	bool m_uploaded = false;
	std::shared_ptr<Mesh> m_mesh;
	ComPtr<ID3D12Resource> m_vertexUploadBuffer;
	ComPtr<ID3D12Resource> m_indexUploadBuffer;
	ComPtr<ID3D12Resource> m_vertexBuffer; // Vertex Buffer stored on GPU (Default Heap)
	ComPtr<ID3D12Resource> m_indexBuffer; // Index Buffer stored on GPU (Default Heap)
	ComPtr<ID3D12Resource> m_worldMatrixBuffer; // stored on CPU (Upload Heap)
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	std::string m_name;
	std::shared_ptr<IMaterialResource> m_material;
	XMMATRIX m_worldMatrix;
};

