#pragma once

#include <vector>
#include "DXAPI/stdafx.h"
#include "Model.h"

using Microsoft::WRL::ComPtr;

class Vertex;

class ModelLoader
{
public:
	static void LoadModel(const char* path, std::vector< Vertex >& vertices, std::vector< UINT >& indices);
};

