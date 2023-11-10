#pragma once

#include "DXAPI/stdafx.h"

using namespace DirectX;

enum MaterialType
{
	Base = 0, // show base color
	Phong,
	Disney,
	Light,
	Count
};

class IMaterial
{
public:
	MaterialType GetType() { return m_type; }
protected:
	MaterialType m_type;

};

class BaseMaterial : public IMaterial
{
public:
	BaseMaterial() {
		m_type = MaterialType::Base;
	}
	~BaseMaterial() {}

public:
};

class PhongMaterial : public IMaterial
{
public:
	PhongMaterial() : ka(0.f), kd(0.f), ks(0.f) {
		m_type = MaterialType::Phong;
	}
	~PhongMaterial() {}

public:
	float ka;
	float kd;
	float ks;
};

class DisneyMaterial : public IMaterial
{
public:
	DisneyMaterial() : 
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
	~DisneyMaterial() {}

public:
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
