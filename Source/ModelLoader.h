#pragma once

#include <vector>
#include "DXAPI/stdafx.h"
#include "Meshes/Mesh.h"

using Microsoft::WRL::ComPtr;

class Vertex;

class ModelLoader
{
public:
	static void LoadModel(const char* path, std::vector< Vertex >& vertices, std::vector< UINT >& indices);
	static void CreatePlane(std::vector< Vertex >& vertices);
	static void CreateTetrahedron(std::vector< Vertex >& vertices, std::vector< UINT >& indices);
};

