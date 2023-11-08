#pragma once

#include "DXAPI/stdafx.h"
#include <vector>
#include <memory>
#include "Materials/Material.h"


using namespace DirectX;

class Vertex
{
public:
    Vertex(XMFLOAT3 pos, XMFLOAT4 col)
        :POSITION(pos), COLOR(col), NORMAL(0, 0, 0), UV(0, 0), TANGENT(0, 0, 0), BITANGENT(0, 0, 0)
    {}

    Vertex(XMFLOAT4 pos, XMFLOAT4 col, XMFLOAT3 n)
        :POSITION(pos.x, pos.y, pos.z), COLOR(col), NORMAL(n), UV(0, 0), TANGENT(0, 0, 0), BITANGENT(0, 0, 0)
    {}

    Vertex(XMFLOAT4 pos, XMFLOAT4 col, XMFLOAT3 n, XMFLOAT2 uv)
        :POSITION(pos.x, pos.y, pos.z), COLOR(col), NORMAL(n), UV(uv.x, uv.y), TANGENT(0, 0, 0), BITANGENT(0, 0, 0)
    {}

    Vertex(XMFLOAT4 pos, XMFLOAT4 col, XMFLOAT3 n, XMFLOAT2 uv, XMFLOAT3 t, XMFLOAT3 b)
		:POSITION(pos.x, pos.y, pos.z), COLOR(col), NORMAL(n), UV(uv.x, uv.y), TANGENT(t), BITANGENT(b)
	{}


    XMFLOAT3 POSITION;
    XMFLOAT4 COLOR;
    XMFLOAT3 NORMAL;
    XMFLOAT2 UV;
    XMFLOAT3 TANGENT;
    XMFLOAT3 BITANGENT;
};

class Mesh
{
public:
    Mesh(std::vector<Vertex>& vertices) {
        m_vertices = std::move(vertices);
        m_vertexCount = m_vertices.size();
        m_indexCount = 0;
        m_verticeOnly = true;
    }

    Mesh(std::vector<Vertex>& vertices, std::vector<UINT>& indices) {
        m_vertices = std::move(vertices);
        m_indices = std::move(indices);
        m_vertexCount = m_vertices.size();
        m_indexCount = m_indices.size();
    }

    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    const std::vector<UINT>& GetIndices() const { return m_indices; }
    const UINT GetVertexCount() const { return m_vertexCount; }
    const UINT GetIndexCount() const { return m_indexCount; }
    const bool IsVerticeOnly() const { return m_verticeOnly; }

    ~Mesh()
    {
	}

private:
	std::vector<Vertex> m_vertices;
	std::vector<UINT> m_indices;

    UINT m_vertexCount;
    UINT m_indexCount;

    bool m_verticeOnly = false;
};

class DisneyMaterialParams
{
public:
    XMFLOAT3 baseColor;
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