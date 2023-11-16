#pragma once

#include "DXAPI/stdafx.h"
#include "GraphicsCore.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using Graphics::g_device;
using Graphics::g_commandAllocator;
using Graphics::g_commandQueue;
using Graphics::g_commandList;


enum MaterialType
{
	Base = 0, // show base color
	Phong,
	Disney,
	Light,
	Count
};

struct IMaterialParams 
{

};

struct BaseMaterialParams : public IMaterialParams
{
	XMFLOAT4 baseColor;
};

struct PhongMaterialParams : public IMaterialParams
{
	XMFLOAT4 kd;
	XMFLOAT4 ka;	// When kd set as float4, cbuffer in HLSL will align to 16 bytes, should padding float3 to float4
	XMFLOAT4 ks;
};

struct DisneyMaterialParams : public IMaterialParams
{
	XMFLOAT4 baseColor;
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

class IMaterialResource
{
public:
	MaterialType GetType() { return m_type; }
	ComPtr<ID3D12Resource> GetMaterialBuffer() { return m_materialBuffer; }

	virtual std::shared_ptr<IMaterialParams> GetMaterialParams() = 0;
	virtual void SetMaterialParams(const IMaterialParams& params) = 0;
	virtual void UploadResource() = 0;
protected:
	virtual void UpdateMaterialBuffer() = 0;
	void CreateUploadBuffer(const void* const initData, const UINT64 byteSize, ComPtr<ID3D12Resource>& buffer);

	MaterialType m_type;
	ComPtr<ID3D12Resource> m_materialBuffer = nullptr;
};

///////////////////////////////////////////
///////// BaseMaterialResource ////////////
///////////////////////////////////////////
class BaseMaterialResource : public IMaterialResource
{
public:
	BaseMaterialResource() : m_baseColor(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)) {
		m_type = MaterialType::Base;
	}

	BaseMaterialResource(const BaseMaterialParams& params) : m_baseColor(params.baseColor) {
		m_type = MaterialType::Base;
	}

	~BaseMaterialResource() {}

	std::shared_ptr<IMaterialParams> GetMaterialParams() override {
		std::shared_ptr<BaseMaterialParams> params = std::make_shared<BaseMaterialParams>();
		params->baseColor = m_baseColor;
		return params;
	}
	void SetMaterialParams(const IMaterialParams& params) override;
	void UploadResource() override;

public:
	void UpdateMaterialBuffer() override;

	XMFLOAT4 m_baseColor;
};

///////////////////////////////////////////
///////// PhongMaterialResource ///////////
///////////////////////////////////////////
class PhongMaterialResource : public IMaterialResource
{
public:
	PhongMaterialResource() : m_kd(0.f, 0.f, 0.f, 1.f), m_ka(0.f, 0.f, 0.f, 1.f), m_ks(0.f, 0.f, 0.f, 1.f) {
		m_type = MaterialType::Phong;
	}

	PhongMaterialResource(const PhongMaterialParams& params) : m_kd(params.kd), m_ka(params.ka), m_ks(params.ks) {
		m_type = MaterialType::Phong;
	}

	~PhongMaterialResource() {}

	std::shared_ptr<IMaterialParams> GetMaterialParams() override {
		std::shared_ptr<PhongMaterialParams> params = std::make_shared<PhongMaterialParams>();
		params->kd = m_kd;
		params->ka = m_ka;
		params->ks = m_ks;
		return params;
	}
	void SetMaterialParams(const IMaterialParams& params) override;
	void UploadResource() override;

public:
	
	void UpdateMaterialBuffer() override;

	XMFLOAT4 m_kd;
	XMFLOAT4 m_ka;
	XMFLOAT4 m_ks;
};

///////////////////////////////////////////
///////// DisneyMaterialResource //////////
///////////////////////////////////////////
class DisneyMaterialResource : public IMaterialResource
{
public:
	DisneyMaterialResource() :
	m_baseColor(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)), 
	m_metallic(0.0f),
	m_subsurface(0.0f), 
	m_specular(0.0f),
	m_roughness(0.0f), 
	m_specularTint(0.0f),
	m_anisotropic(0.0f), 
	m_sheen(0.0f),
	m_sheenTint(0.0f), 
	m_clearcoat(0.0f),
	m_clearcoatGloss(0.0f) {
		m_type = MaterialType::Disney;
	}

	DisneyMaterialResource(const DisneyMaterialParams& params) :
	m_baseColor(params.baseColor),
	m_metallic(params.metallic),
	m_subsurface(params.subsurface),
	m_specular(params.specular),
	m_roughness(params.roughness),
	m_specularTint(params.specularTint),
	m_anisotropic(params.anisotropic),
	m_sheen(params.sheen),
	m_sheenTint(params.sheenTint),
	m_clearcoat(params.clearcoat),
	m_clearcoatGloss(params.clearcoatGloss) {
		m_type = MaterialType::Disney;
	}

	~DisneyMaterialResource() {}

	std::shared_ptr<IMaterialParams> GetMaterialParams() override {
		std::shared_ptr<DisneyMaterialParams> params = std::make_shared<DisneyMaterialParams>();
		params->baseColor = m_baseColor;
		params->metallic = m_metallic;
		params->subsurface = m_subsurface;
		params->specular = m_specular;
		params->roughness = m_roughness;
		params->specularTint = m_specularTint;
		params->anisotropic = m_anisotropic;
		params->sheen = m_sheen;
		params->sheenTint = m_sheenTint;
		params->clearcoat = m_clearcoat;
		params->clearcoatGloss = m_clearcoatGloss;
		return params;
	}
	void SetMaterialParams(const IMaterialParams& params) override;
	void UploadResource() override;

public:
	void UpdateMaterialBuffer() override;

	XMFLOAT4 m_baseColor;
	float m_metallic;
	float m_subsurface;
	float m_specular;
	float m_roughness;
	float m_specularTint;
	float m_anisotropic;
	float m_sheen;
	float m_sheenTint;
	float m_clearcoat;
	float m_clearcoatGloss;

};
