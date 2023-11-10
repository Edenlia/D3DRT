#include "Renderer.h"

void GraphicsRenderer::Draw(const std::shared_ptr<MeshResource> meshResource)
{
    g_commandList->SetGraphicsRootConstantBufferView(2 /*root sig param 2*/, meshResource->GetWorldMatrixBuffer()->GetGPUVirtualAddress());

    switch (meshResource->GetMaterial()->GetType())
    {
        case MaterialType::Base:
        {
            g_commandList->SetPipelineState(m_basePSO.GetPipelineStateObject());
            break;
        }
        case MaterialType::Phong:
        {
            g_commandList->SetPipelineState(m_blinnPhongPSO.GetPipelineStateObject());
            break;
        }
        case MaterialType::Disney:
        {
            g_commandList->SetPipelineState(m_disneyPSO.GetPipelineStateObject());
            break;
        }
    }

    g_commandList->IASetVertexBuffers(0, 1, &meshResource->GetVertexBufferView());
    if (!meshResource->IsVerticeOnly()) {
        g_commandList->IASetIndexBuffer(&meshResource->GetIndexBufferView());
        g_commandList->DrawIndexedInstanced(meshResource->GetIndexCount(), 1, 0, 0, 0);
    }
    else {
        g_commandList->DrawInstanced(meshResource->GetVertexCount(), 1, 0, 0);
    }
}

// Initialize root signature and pipeline state objects
void GraphicsRenderer::Initialize(void)
{
	{
        m_rootSig.Reset(4, 1);
        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy = 0;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        m_rootSig.InitStaticSampler(0, samplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
        m_rootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
        m_rootSig[1].InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        m_rootSig[1].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 /*t0*/, 1, 0);
        m_rootSig[2].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL); // world matrix
        m_rootSig[3].InitAsConstantBuffer(2, D3D12_SHADER_VISIBILITY_ALL); // disney material
        m_rootSig.Finalize(L"RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}

    // Base PSO
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ID3DBlob* vsErrorBlob = nullptr;
        ID3DBlob* psErrorBlob = nullptr;

        HRESULT hr1 = D3DCompileFromFile(L"Shaders/Rasteration/Base.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &vsErrorBlob);
        HRESULT hr2 = D3DCompileFromFile(L"Shaders/Rasteration/Base.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &psErrorBlob);

        if (vsErrorBlob) {
            OutputDebugStringA((char*)vsErrorBlob->GetBufferPointer());
        }
        if (psErrorBlob) {
            OutputDebugStringA((char*)psErrorBlob->GetBufferPointer());
        }

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        m_basePSO = GraphicsPSO(L"Base");
        m_basePSO.SetRootSignature(m_rootSig);
        m_basePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
        m_basePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
        m_basePSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
        m_basePSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
        m_basePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        m_basePSO.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
        m_basePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
        m_basePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
        m_basePSO.Finalize();
    }

    // Blinn-Phong PSO
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ID3DBlob* vsErrorBlob = nullptr;
        ID3DBlob* psErrorBlob = nullptr;

        HRESULT hr1 = D3DCompileFromFile(L"Shaders/Rasteration/BlinnPhong.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &vsErrorBlob);
        HRESULT hr2 = D3DCompileFromFile(L"Shaders/Rasteration/BlinnPhong.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &psErrorBlob);

        if (vsErrorBlob) {
            OutputDebugStringA((char*)vsErrorBlob->GetBufferPointer());
        }
        if (psErrorBlob) {
            OutputDebugStringA((char*)psErrorBlob->GetBufferPointer());
        }

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        m_blinnPhongPSO = GraphicsPSO(L"Blinn Phong");
        m_blinnPhongPSO.SetRootSignature(m_rootSig);
        m_blinnPhongPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
        m_blinnPhongPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
        m_blinnPhongPSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
        m_blinnPhongPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
        m_blinnPhongPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        m_blinnPhongPSO.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
        m_blinnPhongPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
        m_blinnPhongPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
        m_blinnPhongPSO.Finalize();
    }

    // Disney PSO
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ID3DBlob* vsErrorBlob = nullptr;
        ID3DBlob* psErrorBlob = nullptr;

        HRESULT hr1 = D3DCompileFromFile(L"Shaders/Rasteration/DisneyPrinciple.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &vsErrorBlob);
        HRESULT hr2 = D3DCompileFromFile(L"Shaders/Rasteration/DisneyPrinciple.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &psErrorBlob);

        if (vsErrorBlob) {
            OutputDebugStringA((char*)vsErrorBlob->GetBufferPointer());
        }
        if (psErrorBlob) {
            OutputDebugStringA((char*)psErrorBlob->GetBufferPointer());
        }

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        m_disneyPSO = GraphicsPSO(L"Blinn Phong");
        m_disneyPSO.SetRootSignature(m_rootSig);
        m_disneyPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
        m_disneyPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
        m_disneyPSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
        m_disneyPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
        m_disneyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        m_disneyPSO.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
        m_disneyPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
        m_disneyPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
        m_disneyPSO.Finalize();
    }
}
