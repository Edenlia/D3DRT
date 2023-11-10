#pragma once

#include "DXAPI/stdafx.h"
#include "Util/Utility.h"
#include "GraphicsCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "Meshes/MeshResource.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;
using Graphics::g_device;
using Graphics::g_commandAllocator;
using Graphics::g_commandQueue;
using Graphics::g_commandList;

class IRenderer
{
public:
	virtual void Draw(const std::shared_ptr<MeshResource> meshResource) = 0;
	virtual void Initialize(void) = 0;

	virtual ID3D12RootSignature* GetRootSignature() = 0;
	virtual ID3D12PipelineState* GetDefaultPSO() = 0;
};

class GraphicsRenderer : public IRenderer
{
public:
	void Draw(const std::shared_ptr<MeshResource> meshResource) override;

	void Initialize(void) override;

	ID3D12RootSignature* GetRootSignature() { return m_rootSig.GetSignature(); }
	ID3D12PipelineState* GetDefaultPSO() { return m_basePSO.GetPipelineStateObject(); }

private:
	RootSignature m_rootSig;

	GraphicsPSO m_basePSO;
	GraphicsPSO m_blinnPhongPSO;
	GraphicsPSO m_disneyPSO;
	
};
