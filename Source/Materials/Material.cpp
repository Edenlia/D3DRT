#include "Material.h"
#include "Util/Utility.h"


void IMaterialResource::CreateUploadBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& buffer) {
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
    ThrowIfFailed(buffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, initData, byteSize);
    buffer->Unmap(0, nullptr);
}

///////////////////////////////////////////
///////// BaseMaterialResource ////////////
///////////////////////////////////////////
void BaseMaterialResource::SetMaterialParams(const IMaterialParams& params)
{
	const BaseMaterialParams& baseParams = static_cast<const BaseMaterialParams&>(params);
	m_baseColor = baseParams.baseColor;

	UpdateMaterialBuffer();
}

void BaseMaterialResource::UploadResource() 
{
    BaseMaterialParams params = {};
    params.baseColor = m_baseColor;

    // Create constant buffer
	CreateUploadBuffer(&params, sizeof(BaseMaterialParams), m_materialBuffer);
}

void BaseMaterialResource::UpdateMaterialBuffer() 
{
	BaseMaterialParams params = {};
	params.baseColor = m_baseColor;

	// Copy the matrix contents
	uint8_t* pData;
	ThrowIfFailed(m_materialBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, &params, sizeof(BaseMaterialParams));
	m_materialBuffer->Unmap(0, nullptr);
}


///////////////////////////////////////////
///////// PhongMaterialResource ///////////
///////////////////////////////////////////
void PhongMaterialResource::SetMaterialParams(const IMaterialParams& params)
{
	const PhongMaterialParams& phongParams = static_cast<const PhongMaterialParams&>(params);
	m_kd = phongParams.kd;
	m_ka = phongParams.ka;
	m_ks = phongParams.ks;

	UpdateMaterialBuffer();
}

void PhongMaterialResource::UploadResource() 
{
	PhongMaterialParams params = {};
	params.kd = m_kd;
	params.ka = m_ka;
    params.ks = m_ks;

	// Create constant buffer
	CreateUploadBuffer(&params, sizeof(PhongMaterialParams), m_materialBuffer);
}

void PhongMaterialResource::UpdateMaterialBuffer() 
{
	PhongMaterialParams params = {};
	params.kd = m_kd;
	params.ka = m_ka;
	params.ks = m_ks;

	// Copy the matrix contents
	uint8_t* pData;
	ThrowIfFailed(m_materialBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, &params, sizeof(PhongMaterialParams));
	m_materialBuffer->Unmap(0, nullptr);
}

///////////////////////////////////////////
///////// DisneyMaterialResource //////////
///////////////////////////////////////////
void DisneyMaterialResource::SetMaterialParams(const IMaterialParams& params)
{
	const DisneyMaterialParams& disneyParams = static_cast<const DisneyMaterialParams&>(params);
	m_baseColor = disneyParams.baseColor;
	m_subsurface = disneyParams.subsurface;
	m_metallic = disneyParams.metallic;
	m_specular = disneyParams.specular;
	m_specularTint = disneyParams.specularTint;
	m_anisotropic = disneyParams.anisotropic;
	m_roughness = disneyParams.roughness;
	m_sheen = disneyParams.sheen;
	m_sheenTint = disneyParams.sheenTint;
	m_clearcoat = disneyParams.clearcoat;
	m_clearcoatGloss = disneyParams.clearcoatGloss;

	UpdateMaterialBuffer();
}

void DisneyMaterialResource::UploadResource() 
{
	DisneyMaterialParams params = {};
	params.baseColor = m_baseColor;
	params.subsurface = m_subsurface;
	params.metallic = m_metallic;
	params.specular = m_specular;
	params.specularTint = m_specularTint;
	params.anisotropic = m_anisotropic;
	params.roughness = m_roughness;
	params.sheen = m_sheen;
	params.sheenTint = m_sheenTint;
	params.clearcoat = m_clearcoat;
	params.clearcoatGloss = m_clearcoatGloss;

	// Create constant buffer
	CreateUploadBuffer(&params, sizeof(DisneyMaterialParams), m_materialBuffer);
}

void DisneyMaterialResource::UpdateMaterialBuffer() 
{
	DisneyMaterialParams params = {};
	params.baseColor = m_baseColor;
	params.subsurface = m_subsurface;
	params.metallic = m_metallic;
	params.specular = m_specular;
	params.specularTint = m_specularTint;
	params.anisotropic = m_anisotropic;
	params.roughness = m_roughness;
	params.sheen = m_sheen;
	params.sheenTint = m_sheenTint;
	params.clearcoat = m_clearcoat;
	params.clearcoatGloss = m_clearcoatGloss;

	// Copy the matrix contents
	uint8_t* pData;
	ThrowIfFailed(m_materialBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, &params, sizeof(DisneyMaterialParams));
	m_materialBuffer->Unmap(0, nullptr);
}
